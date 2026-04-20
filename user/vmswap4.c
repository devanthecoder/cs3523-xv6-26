// vmswap4.c -- Fork with pages in swap.
// The parent writes enough pages to trigger eviction (some pages go to
// swap), then calls fork(). uvmcopy must handle swapped-out PTEs by
// swapping them in before copying. Both parent and child then verify
// their independent copies of the data.
#include "kernel/types.h"
#include "kernel/vminfo.h"
#include "user/user.h"

#define PGSIZE 4096
#define NPAGES 70   // enough to push pages into swap before fork

#define PATTERN(i) ((char)((i * 13 + 3) & 0xFF))

int
main(void)
{
    printf("=== vmswap4: Fork with Pages in Swap ===\n");

    char *mem = sbrklazy(NPAGES * PGSIZE);
    if (mem == SBRK_ERROR) {
        printf("FAIL: parent sbrklazy failed\n");
        exit(1);
    }

    // Write phase: fills frames and pushes early pages to swap
    for (int i = 0; i < NPAGES; i++) {
        mem[i * PGSIZE] = PATTERN(i);
    }

    // Fork -- child inherits the full address space, including swapped pages
    int pid = fork();
    if (pid < 0) {
        printf("FAIL: fork() returned error\n");
        exit(1);
    }

    if (pid == 0) {
        // ---- CHILD ----
        int ok = 1;
        for (int i = 0; i < NPAGES; i++) {
            if (mem[i * PGSIZE] != PATTERN(i)) {
                printf("FAIL: child page %d wrong (expected 0x%x got 0x%x)\n",
                       i, (unsigned char)PATTERN(i),
                       (unsigned char)mem[i * PGSIZE]);
                ok = 0;
            }
        }
        if (ok)
            printf("PASS: child sees correct data for all %d pages post-fork\n",
                   NPAGES);
        else
            printf("FAIL: child detected data corruption after fork\n");

        // Mutate child's copy -- should not affect parent
        for (int i = 0; i < NPAGES; i++) {
            mem[i * PGSIZE] = ~PATTERN(i);
        }
        printf("PASS: child mutation complete (COW would be tested separately)\n");

        struct vmstats cs;
        getvmstats(getpid(), &cs);
        printf("INFO: child -- faults=%d evicted=%d swapped_out=%d swapped_in=%d resident=%d\n",
               cs.page_faults, cs.pages_evicted,
               cs.pages_swapped_out, cs.pages_swapped_in, cs.resident_pages);
        exit(ok ? 0 : 1);

    } else {
        // ---- PARENT ----
        // Wait for child first, then re-verify parent's own data
        int child_status = -1;
        wait(&child_status);

        int ok = 1;
        for (int i = 0; i < NPAGES; i++) {
            if (mem[i * PGSIZE] != PATTERN(i)) {
                printf("FAIL: parent page %d corrupted after child ran\n", i);
                ok = 0;
            }
        }
        if (ok)
            printf("PASS: parent data intact after child mutation\n");
        else
            printf("FAIL: parent data was corrupted by child\n");

        if (child_status == 0)
            printf("PASS: child exited successfully\n");
        else
            printf("FAIL: child exited with status %d\n", child_status);

        struct vmstats ps;
        getvmstats(getpid(), &ps);
        printf("INFO: parent -- faults=%d evicted=%d swapped_out=%d swapped_in=%d resident=%d\n",
               ps.page_faults, ps.pages_evicted,
               ps.pages_swapped_out, ps.pages_swapped_in, ps.resident_pages);
    }

    exit(0);
}
