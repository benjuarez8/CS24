#define _GNU_SOURCE 1

#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "util.h"

static const size_t PAGE_SIZE = 4096;
typedef uint8_t page_t[PAGE_SIZE];

static void *const START_PAGE = (void *) ((size_t) 1 << 32);
static const size_t MAX_HEAP_SIZE = (size_t) 1 << 30;
static const int HEAP_MMAP_FLAGS = MAP_ANONYMOUS | MAP_PRIVATE;
static const size_t HEADER_MAGIC = 0x0123456789ABCDEF;

typedef struct {
    size_t magic;
    size_t size;
    bool is_allocated;
} header_t;

static bool is_initialized = false;
static page_t *current_page;

static size_t pages_round_up(size_t size) {
    return (size + PAGE_SIZE - 1) / PAGE_SIZE;
}

static void set_header(page_t *header_page, size_t size, bool is_allocated) {
    header_t *header = (header_t *) header_page;
    mprotect((void *) header, PAGE_SIZE, PROT_READ | PROT_WRITE);
    header->magic = HEADER_MAGIC;
    header->size = size;
    header->is_allocated = is_allocated;
    mprotect((void *) header, PAGE_SIZE, PROT_NONE);
}

static void *get_payload(page_t *header_page, size_t size) {
    void *payload_end =
        (void *) header_page + PAGE_SIZE + pages_round_up(size) * PAGE_SIZE;
    return payload_end - size;
}

static void check_for_leaks(void) {
    // Prevent memory leaks from stdout
    fclose(stdout);
    header_t *header = START_PAGE;
    header_t *next_header;
    while ((page_t *) header < current_page) {
        mprotect((void *) header, PAGE_SIZE, PROT_READ | PROT_WRITE);
        if (header->is_allocated) {
            void *allocation = get_payload((page_t *) header, header->size);
            size_t allocation_size = header->size;
            report_memory_leak(allocation, allocation_size);
        }
        next_header =
            (void *) header + PAGE_SIZE + pages_round_up(header->size) * PAGE_SIZE;
        mprotect((void *) header, PAGE_SIZE, PROT_NONE);
        header = next_header;
    }
}

static void sigsegv_handler(int signum, siginfo_t *siginfo, void *context) {
    (void) context;
    (void) signum;
    void *address = siginfo->si_addr;
    if (START_PAGE <= address && address < (void *) (current_page + 1)) {
        report_invalid_heap_access(address);
    }
    else {
        report_seg_fault(address);
    }
}

static void asan_init(void) {
    struct sigaction act = {.sa_sigaction = sigsegv_handler, .sa_flags = SA_SIGINFO};
    sigaction(SIGSEGV, &act, NULL);

    if (is_initialized) {
        return;
    }

    // Avoid buffering on stdout
    setbuf(stdout, NULL);

    current_page = mmap(START_PAGE, MAX_HEAP_SIZE, PROT_NONE, HEAP_MMAP_FLAGS, -1, 0);
    assert(current_page == START_PAGE);

    atexit(check_for_leaks);

    is_initialized = true;
}

void *malloc(size_t size) {
    asan_init();

    size_t pages_necessary = pages_round_up(size);

    // Store the size of the allocation at the beginning of the page before the payload
    page_t *header_page = current_page;
    set_header(header_page, size, true);
    current_page += 1 + pages_necessary;
    mprotect((void *) header_page + PAGE_SIZE, pages_necessary * PAGE_SIZE,
             PROT_READ | PROT_WRITE);

    // Provide the user with the END of the first page
    return get_payload(header_page, size);
}

void free(void *ptr) {
    asan_init();

    if (ptr == NULL) {
        return;
    }

    header_t *header = ptr - PAGE_SIZE - ((size_t) ptr % PAGE_SIZE);
    mprotect((void *) header, PAGE_SIZE, PROT_READ | PROT_WRITE);
    page_t *header_page = (page_t *) header;

    bool oob_lower = ptr < START_PAGE + PAGE_SIZE;
    bool oob_upper = ptr > (void *) current_page;
    if (oob_lower || oob_upper) {
        report_invalid_free(ptr);
    }

    bool no_magic = header->magic != HEADER_MAGIC;
    bool not_payload_beg = ptr != get_payload(header_page, header->size);
    if (no_magic || not_payload_beg) {
        report_invalid_free(ptr);
    }

    if (!header->is_allocated) {
        report_double_free(ptr, header->size);
    }
    else {
        header->is_allocated = false;
    }

    mprotect((void *) header, PAGE_SIZE + pages_round_up(header->size) * PAGE_SIZE,
             PROT_NONE);
}
