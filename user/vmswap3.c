// vmswap3.c -- Full-page data integrity test.
// Unlike vmswap2 which only checks the first byte of each page, this
// test writes a distinct pattern to EVERY byte within each page, then
// reads them all back. This catches partial-page corruption that a
// first-byte-only check would miss (e.g., off-by-one in memmove sizes).
#include "kernel/types.h"
#include "kernel/vminfo.h"
#include "user/user.h"

#define PGSIZE  4096
#define NPAGES  70

// Per-byte pattern: combines page index and byte offset so every byte
// in every page is unique.
#define PATTERN(page, off) ((char)(((page) * 7 + (off)) & 0xFF))

int
main(void)
{
    printf("=== vmswap3: Full-Page Byte-Level Integrity ===\n");

    char *mem = sbrklazy(NPAGES * PGSIZE);
    if (mem == SBRK_ERROR) {
        printf("FAIL: sbrklazy failed\n");
        exit(1);
    }

    // Write distinct pattern to every byte of every page
    for (int i = 0; i < NPAGES; i++) {
        char *page = mem + (i * PGSIZE);
        for (int b = 0; b < PGSIZE; b++) {
            page[b] = PATTERN(i, b);
        }
    }

    // Read back and verify every byte
    int ok = 1;
    int first_bad_page = -1;
    int first_bad_byte = -1;

    for (int i = 0; i < NPAGES; i++) {
        char *page = mem + (i * PGSIZE);
        for (int b = 0; b < PGSIZE; b++) {
            if (page[b] != PATTERN(i, b)) {
                if (ok) {
                    first_bad_page = i;
                    first_bad_byte = b;
                }
                ok = 0;
            }
        }
    }

    if (ok) {
        printf("PASS: all %d pages x %d bytes intact (full-page check)\n",
               NPAGES, PGSIZE);
    } else {
        printf("FAIL: corruption at page %d byte %d\n",
               first_bad_page, first_bad_byte);
        char *page = mem + (first_bad_page * PGSIZE);
        printf("FAIL: expected 0x%x got 0x%x\n",
               (unsigned char)PATTERN(first_bad_page, first_bad_byte),
               (unsigned char)page[first_bad_byte]);
    }

    // Stride access: jump through memory non-sequentially.
    // This exercises swap-in of scattered pages.
    ok = 1;
    for (int stride = 7; stride < NPAGES; stride += 7) {
        char *page = mem + (stride * PGSIZE);
        for (int b = 0; b < PGSIZE; b++) {
            if (page[b] != PATTERN(stride, b)) {
                ok = 0;
                printf("FAIL: stride access: page %d byte %d corrupted\n",
                       stride, b);
                break;
            }
        }
        if (!ok) break;
    }

    if (ok)
        printf("PASS: stride (non-sequential) access preserves all page data\n");
    else
        printf("FAIL: stride access detected corruption\n");

    struct vmstats s;
    getvmstats(getpid(), &s);
    printf("INFO: faults=%d evicted=%d swapped_out=%d swapped_in=%d resident=%d\n",
           s.page_faults, s.pages_evicted,
           s.pages_swapped_out, s.pages_swapped_in, s.resident_pages);

    exit(0);
}
