// vmswap1.c -- Basic lazy allocation and page fault test.
// Verifies that sbrklazy triggers page faults on first access,
// that page_faults and resident_pages are tracked correctly,
// and that data written through lazy pages is preserved.
#include "kernel/types.h"
#include "kernel/vminfo.h"
#include "user/user.h"

#define PGSIZE 4096
#define NPAGES 20

int
main(void)
{
    printf("=== vmswap1: Basic Lazy Alloc & Page Fault ===\n");

    int pid = getpid();
    struct vmstats before, after;

    if (getvmstats(pid, &before) < 0) {
        printf("FAIL: getvmstats (before) returned error\n");
        exit(1);
    }

    // Lazily allocate NPAGES pages -- no physical frames yet
    char *mem = sbrklazy(NPAGES * PGSIZE);
    if (mem == SBRK_ERROR) {
        printf("FAIL: sbrklazy returned error\n");
        exit(1);
    }

    // Touch each page -- each first access must trigger a page fault
    for (int i = 0; i < NPAGES; i++) {
        mem[i * PGSIZE] = (char)(i + 1);
    }

    // Verify written values are still intact
    int ok = 1;
    for (int i = 0; i < NPAGES; i++) {
        if (mem[i * PGSIZE] != (char)(i + 1)) {
            printf("FAIL: page %d corrupted (expected %d got %d)\n",
                   i, (char)(i + 1), (unsigned char)mem[i * PGSIZE]);
            ok = 0;
        }
    }

    if (ok)
        printf("PASS: all %d pages readable after lazy page faults\n", NPAGES);
    else
        printf("FAIL: data corruption detected in lazy pages\n");

    if (getvmstats(pid, &after) < 0) {
        printf("FAIL: getvmstats (after) returned error\n");
        exit(1);
    }

    int faults_added = after.page_faults - before.page_faults;
    if (faults_added >= NPAGES)
        printf("PASS: page_faults increased by at least %d (got +%d)\n",
               NPAGES, faults_added);
    else
        printf("FAIL: expected page_faults += %d, got +%d\n",
               NPAGES, faults_added);

    int resident_added = after.resident_pages - before.resident_pages;
    if (resident_added > 0)
        printf("PASS: resident_pages increased by %d\n", resident_added);
    else
        printf("FAIL: resident_pages did not increase (delta=%d)\n",
               resident_added);

    printf("INFO: faults=%d evicted=%d swapped_out=%d swapped_in=%d resident=%d\n",
           after.page_faults, after.pages_evicted,
           after.pages_swapped_out, after.pages_swapped_in, after.resident_pages);

    exit(0);
}
