#include "cache_timing.h"

#include <stdio.h>
#include <stdlib.h>

#include "util.h"

const size_t REPEATS = 100000;

int main() {
    uint64_t sum_miss = 0;
    uint64_t sum_hit = 0;

    for (size_t i = 0; i < REPEATS; i++) {
        page_t *pages = calloc(UINT8_MAX + 1, PAGE_SIZE);
        flush_cache_line(pages);
        uint64_t first = time_read(pages);
        uint64_t second = time_read(pages);
        if (first >= second) {
            sum_miss = sum_miss + first;
            sum_hit = sum_hit + second;
        }
    }

    printf("average miss = %" PRIu64 "\n", sum_miss / REPEATS);
    printf("average hit  = %" PRIu64 "\n", sum_hit / REPEATS);
}
