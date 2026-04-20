// vmswap2.c -- Data integrity test across swap-out and swap-in.
// Allocates more pages than NFRAME=64 to force eviction, writes a
// distinct per-page pattern, then reads all pages back to verify
// that swap_out and swap_in preserve data correctly.
#include "kernel/types.h"
#include "kernel/vminfo.h"
#include "user/user.h"

#define PGSIZE  4096
#define NPAGES  80   // exceeds NFRAME=64 by design -- eviction is guaranteed

// Unique byte pattern for page i (XOR trick keeps it non-zero)
#define PATTERN(i) ((char)((i ^ 0xA5) & 0xFF))

int
main(void)
{
    printf("=== vmswap2: Swap-Out / Swap-In Data Integrity ===\n");

    int pid = getpid();
    struct vmstats before, after;
    getvmstats(pid, &before);

    char *mem = sbrklazy(NPAGES * PGSIZE);
    if (mem == SBRK_ERROR) {
        printf("FAIL: sbrklazy failed for %d pages\n", NPAGES);
        exit(1);
    }

    // Write phase: touching pages sequentially forces early pages to be
    // evicted (swapped out) as the frame table fills up.
    for (int i = 0; i < NPAGES; i++) {
        mem[i * PGSIZE] = PATTERN(i);
    }

    // Read phase: accessing early pages brings them back from swap.
    int ok = 1;
    for (int i = 0; i < NPAGES; i++) {
        char got = mem[i * PGSIZE];
        if (got != PATTERN(i)) {
            printf("FAIL: page %d -- expected 0x%x got 0x%x\n",
                   i, (unsigned char)PATTERN(i), (unsigned char)got);
            ok = 0;
        }
    }

    if (ok)
        printf("PASS: all %d pages intact after eviction/swap-in\n", NPAGES);
    else
        printf("FAIL: data corruption detected across swap\n");

    getvmstats(pid, &after);

    int swapped_out = after.pages_swapped_out - before.pages_swapped_out;
    if (swapped_out > 0)
        printf("PASS: pages_swapped_out increased by %d\n", swapped_out);
    else
        printf("FAIL: pages_swapped_out did not increase (may mean no eviction occurred)\n");

    int swapped_in = after.pages_swapped_in - before.pages_swapped_in;
    if (swapped_in > 0)
        printf("PASS: pages_swapped_in increased by %d\n", swapped_in);
    else
        printf("FAIL: pages_swapped_in did not increase\n");

    printf("INFO: faults=%d evicted=%d swapped_out=%d swapped_in=%d resident=%d\n",
           after.page_faults, after.pages_evicted,
           after.pages_swapped_out, after.pages_swapped_in, after.resident_pages);

    exit(0);
}
