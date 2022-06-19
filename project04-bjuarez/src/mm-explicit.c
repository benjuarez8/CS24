/*
 * mm-explicit.c - The best malloc package EVAR!
 *
 * TODO (bug): Uh..this is an implicit list???
 */

#include <stdint.h>
#include <string.h>

#include "memlib.h"
#include "mm.h"

/** The required alignment of heap payloads */
const size_t ALIGNMENT = 2 * sizeof(size_t);

/** The layout of each block allocated on the heap */
typedef struct {
    /** The size of the block and whether it is allocated (stored in the low bit) */
    size_t header;
    /**
     * We don't know what the size of the payload will be, so we will
     * declare it as a zero-length array.  This allow us to obtain a
     * pointer to the start of the payload.
     */
    uint8_t payload[];
} block_t;

// Represents our free blocks w/ next/prev pointers
typedef struct {
    size_t header;
    block_t *next;
    block_t *prev;
} free_block_t;

// Represents necessary footer implementation
typedef struct {
    size_t size;
} footer;

// Represents "front" of our free list
static free_block_t *mm_head = NULL;

// Adding to free list
void add(block_t *block) {
    if (block != NULL) {
        free_block_t *freed_block = (free_block_t *) block; 
        if (mm_head == NULL) {
            mm_head = freed_block;
            mm_head->next = NULL;
            mm_head->prev = NULL;
        } else {
            // puts newly freed block at front of free list
            free_block_t *old_head = mm_head;
            mm_head = freed_block;
            mm_head->next = (block_t *) old_head;
            mm_head->prev = NULL;
            old_head->prev = (block_t *) mm_head;
        }
    }
}

// Removing from free list
void remove(block_t *block) {
    if (block != NULL) {
        free_block_t *removed_block = (free_block_t *) block;
        free_block_t *next_block = (free_block_t *) removed_block->next;
        free_block_t *prev_block = (free_block_t *) removed_block->prev;
        if (prev_block != NULL) {
            prev_block->next = (block_t *) next_block;
        } else {
            mm_head = next_block;
        }
        if (next_block != NULL) {
            next_block->prev = (block_t *) prev_block;
        }
    }
}

/** The first and last blocks on the heap */
static block_t *mm_heap_first = NULL;
static block_t *mm_heap_last = NULL;

/** Rounds up `size` to the nearest multiple of `n` */
static size_t round_up(size_t size, size_t n) {
    return (size + (n - 1)) / n * n;
}

/** Set's a block's header with the given size and allocation state */
static void set_header(block_t *block, size_t size, bool is_allocated) {
    block->header = size | is_allocated;
    // Accounts for footer implementation
    footer *foot = (void *) block + size - sizeof(footer);
    foot->size = size;
}

/** Extracts a block's size from its header */
static size_t get_size(block_t *block) {
    return block->header & ~1;
}

/** Extracts a block's allocation state from its header */
static bool is_allocated(block_t *block) {
    return block->header & 1;
}

/**
 * Finds the first free block in the heap with at least the given size.
 * If no block is large enough, returns NULL.
 * 
 * Modified to traverse free list appropriately.
 */
static block_t *find_fit(size_t size) {
    free_block_t *free_block = mm_head;
    while (free_block != NULL) {
        block_t *curr = (block_t *) free_block;
        if (size <= get_size(curr)) {
            return curr;
        }
        free_block = (free_block_t *) free_block->next;
    }
    return NULL;
}

/** Gets the header corresponding to a given payload pointer */
static block_t *block_from_payload(void *ptr) {
    return ptr - offsetof(block_t, payload);
}

/**
 * mm_init - Initializes the allocator state
 */
bool mm_init(void) {
    // We want the first payload to start at ALIGNMENT bytes from the start of the heap
    void *padding = mem_sbrk(ALIGNMENT - sizeof(block_t));
    if (padding == (void *) -1) {
        return false;
    }

    // Initialize the heap with no blocks
    mm_heap_first = NULL;
    mm_heap_last = NULL;
    // Initialized front of free list
    mm_head = NULL;
    return true;
}

/**
 * mm_malloc - Allocates a block with the given size
 */
void *mm_malloc(size_t size) {
    // The block must have enough space for a header and be 16-byte aligned
    // AND footer
    size = round_up(sizeof(block_t) + size + sizeof(footer), ALIGNMENT);

    // If there is a large enough free block, use it
    block_t *block = find_fit(size);
    if (block != NULL) {
        remove(block);
        if (get_size(block) > sizeof(block_t) + size + sizeof(footer)) { 
            block_t *block_split = (void *) block + size;
            size_t split_size = get_size(block) - size;
            if (block == mm_heap_last) {
                mm_heap_last = block_split;
            }
            set_header(block, size, true);
            set_header(block_split, split_size, false);
            add(block_split);
        } else {
            set_header(block, get_size(block), true);
        }
        return block->payload;
    }

    // Otherwise, a new block needs to be allocated at the end of the heap
    block = mem_sbrk(size);
    if (block == (void *) -1) {
        return NULL;
    }

    // Update mm_heap_first and mm_heap_last since we extended the heap
    if (mm_heap_first == NULL) {
        mm_heap_first = block;
    }
    mm_heap_last = block;

    // Initialize the block with the allocated size
    set_header(block, size, true);
    return block->payload;
}

/**
 * mm_free - Releases a block to be reused for future allocations
 */
void mm_free(void *ptr) {
    // mm_free(NULL) does nothing
    if (ptr == NULL) {
        return;
    }
    
    block_t *block = block_from_payload(ptr);
    size_t fsize = get_size(block);
    add(block);

    block_t *left_block = NULL;
    block_t *right_block = NULL;
    
    if (block != mm_heap_first) {
        footer *foot = (void *) block - sizeof(footer);
        left_block = (void *) block - foot->size;
    } 
    if (block != mm_heap_last) {
        right_block = (void *) block + get_size(block);
    }
    
    bool left_free = left_block != NULL && !is_allocated(left_block);
    bool right_free = right_block != NULL && !is_allocated(right_block);
    bool left_right_free = left_free && right_free;

    if (left_right_free) {
        fsize += get_size(left_block) + get_size(right_block);
        set_header(left_block, fsize, false);
        if (right_block == mm_heap_last) {
            mm_heap_last = left_block;
        }
        remove(block);
        remove(right_block);
    } else if (left_free) {
        fsize += get_size(left_block);
        set_header(left_block, fsize, false);
        if (block == mm_heap_last) {
            mm_heap_last = left_block;
        }
        remove(block);
    } else if (right_free) {
        fsize += get_size(right_block);
        set_header(block, fsize, false);
        if (right_block == mm_heap_last) {
            mm_heap_last = block;
        }
        remove(right_block);
    } else {
        set_header(block, fsize, false);
    }
}

/**
 * mm_realloc - Change the size of the block by mm_mallocing a new block,
 *      copying its data, and mm_freeing the old block.
 */
void *mm_realloc(void *old_ptr, size_t size) {
    uint8_t *ptr = mm_malloc(size);
    if (old_ptr == NULL) {
        return ptr;
    } else if (size == 0) {
        mm_free(old_ptr);
        return NULL;
    } 
    size_t new_size = get_size(block_from_payload(old_ptr)) - sizeof(block_t) - sizeof(footer);
    if (new_size > size) {
        memcpy(ptr, old_ptr, size);
    } else {
        memcpy(ptr, old_ptr, new_size);
    }
    mm_free(old_ptr);
    return ptr;
}

/**
 * mm_calloc - Allocate the block and set it to zero.
 */
void *mm_calloc(size_t nmemb, size_t size) {
    block_t *block = mm_malloc(nmemb * size);
    memset(block, 0, nmemb * size);
    return block;
}

/**
 * mm_checkheap - So simple, it doesn't need a checker!
 */
void mm_checkheap(void) {
}
