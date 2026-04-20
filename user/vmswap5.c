// vmswap5.c -- Multi-process memory pressure and stats validation.
// Spawns several children that each allocate a significant chunk of
// memory, placing heavy combined pressure on the 64-frame system.
// Children send their results to the parent via a pipe so that only
// the parent prints -- avoiding interleaved console output.
#include "kernel/types.h"
#include "kernel/vminfo.h"
#include "user/user.h"

#define PGSIZE     4096
#define NPAGES     35    // per child; 3 children = 105 pages >> NFRAME=64
#define NCHILDREN  3

// Each child uses a different pattern so cross-contamination is visible
#define PATTERN(child, page) ((char)((child) * 37 + (page) * 11 + 1))

// Result struct sent from each child to the parent through a pipe
struct child_result {
    int id;
    int ok;
    struct vmstats s;
};

int
main(void)
{
    printf("=== vmswap5: Multi-Process Memory Pressure ===\n");

    int pipefds[NCHILDREN][2];
    int pids[NCHILDREN];

    for (int i = 0; i < NCHILDREN; i++) {
        if (pipe(pipefds[i]) < 0) {
            printf("FAIL: pipe %d failed\n", i);
            exit(1);
        }
        pids[i] = fork();
        if (pids[i] < 0) {
            printf("FAIL: fork %d failed\n", i);
            exit(1);
        }
        if (pids[i] == 0) {
            // ---- CHILD ----
            close(pipefds[i][0]); // child doesn't read

            char *mem = sbrklazy(NPAGES * PGSIZE);
            struct child_result r;
            r.id = i;
            r.ok = 1;

            if (mem == SBRK_ERROR) {
                r.ok = 0;
            } else {
                for (int p = 0; p < NPAGES; p++)
                    mem[p * PGSIZE] = PATTERN(i, p);

                for (int p = 0; p < NPAGES; p++) {
                    if (mem[p * PGSIZE] != PATTERN(i, p)) {
                        r.ok = 0;
                        break;
                    }
                }
            }

            getvmstats(getpid(), &r.s);
            write(pipefds[i][1], &r, sizeof(r));
            close(pipefds[i][1]);
            exit(r.ok ? 0 : 1);
        }

        // Parent closes write end of this child's pipe
        close(pipefds[i][1]);
    }

    // Wait for all children and read their results
    int all_ok = 1;
    for (int i = 0; i < NCHILDREN; i++) {
        int status = -1;
        wait(&status);

        struct child_result r;
        read(pipefds[i][0], &r, sizeof(r));
        close(pipefds[i][0]);

        if (r.ok)
            printf("PASS: child %d all %d pages correct under pressure\n",
                   r.id, NPAGES);
        else
            printf("FAIL: child %d data corrupted under pressure\n", r.id);

        printf("INFO: child %d -- faults=%d evicted=%d swapped_out=%d swapped_in=%d resident=%d\n",
               r.id, r.s.page_faults, r.s.pages_evicted,
               r.s.pages_swapped_out, r.s.pages_swapped_in, r.s.resident_pages);

        if (status != 0) all_ok = 0;
    }

    if (all_ok)
        printf("PASS: all %d children survived memory pressure with correct data\n",
               NCHILDREN);
    else
        printf("FAIL: one or more children reported corruption under pressure\n");

    struct vmstats ps;
    getvmstats(getpid(), &ps);
    printf("INFO: parent -- faults=%d evicted=%d swapped_out=%d swapped_in=%d resident=%d\n",
           ps.page_faults, ps.pages_evicted,
           ps.pages_swapped_out, ps.pages_swapped_in, ps.resident_pages);

    exit(0);
}
