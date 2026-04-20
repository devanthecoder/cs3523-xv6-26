// vmswap6.c -- Scheduler-aware eviction test.
// Verifies that the Clock algorithm preferentially evicts pages
// belonging to lower-priority (higher MLFQ level number) processes.
//
// Child 0 burns CPU to reach MLFQ level 3 before allocating memory.
// Child 1 makes many syscalls to stay at level 0-1 before allocating.
// Both then allocate the same number of pages simultaneously under
// mutual frame pressure. The clock should steal child 0's frames
// (higher level = lower priority) before child 1's.
//
// Synchronization: each child signals its "ready" state via a pipe
// after finishing the burn phase. The parent signals "go" to both
// simultaneously so that memory allocation overlaps on separate CPUs.
#include "kernel/types.h"
#include "kernel/vminfo.h"
#include "user/user.h"

#define PGSIZE     4096
#define NPAGES     40      // 2 children = 80 pages > NFRAME=64
#define CPU_ITERS  300000000L
#define SYS_ITERS  25000

struct result {
    int mlfq_level;
    struct vmstats s;
};

int
main(void)
{
    printf("=== vmswap6: Scheduler-Aware Eviction ===\n");

    // ready[i]: child i writes 1 byte when burn phase done
    // go[i]:    parent writes 1 byte to release child i into alloc phase
    int ready[2][2], go[2][2], res[2][2];
    for (int i = 0; i < 2; i++) {
        if (pipe(ready[i]) < 0 || pipe(go[i]) < 0 || pipe(res[i]) < 0) {
            printf("FAIL: pipe failed\n");
            exit(1);
        }
    }

    // ---- Child 0: CPU-bound ----
    int pid0 = fork();
    if (pid0 < 0) { printf("FAIL: fork 0\n"); exit(1); }
    if (pid0 == 0) {
        // close fds this child doesn't use
        close(ready[0][0]); close(go[0][1]); close(res[0][0]);
        close(ready[1][0]); close(ready[1][1]);
        close(go[1][0]);    close(go[1][1]);
        close(res[1][0]);   close(res[1][1]);

        // burn CPU to sink to MLFQ level 3
        for (volatile long j = 0; j < CPU_ITERS; j++);

        // signal parent: burn done
        char c = 1;
        write(ready[0][1], &c, 1);
        close(ready[0][1]);

        // wait for parent's go
        read(go[0][0], &c, 1);
        close(go[0][0]);

        // allocate under pressure
        char *mem = sbrklazy(NPAGES * PGSIZE);
        if (mem == SBRK_ERROR) { exit(1); }
        for (int i = 0; i < NPAGES; i++) mem[i * PGSIZE] = (char)(i + 1);
        for (int i = 0; i < NPAGES; i++) (void)mem[i * PGSIZE];

        struct result r;
        r.mlfq_level = getlevel();
        getvmstats(getpid(), &r.s);
        write(res[0][1], &r, sizeof(r));
        close(res[0][1]);
        exit(0);
    }
    close(ready[0][1]); close(go[0][0]); close(res[0][1]);

    // ---- Child 1: syscall-heavy ----
    int pid1 = fork();
    if (pid1 < 0) { printf("FAIL: fork 1\n"); exit(1); }
    if (pid1 == 0) {
        // close fds this child doesn't use
        close(ready[1][0]); close(go[1][1]); close(res[1][0]);
        close(ready[0][0]); close(ready[0][1]);
        close(go[0][0]);    close(go[0][1]);
        close(res[0][0]);   close(res[0][1]);

        // stay at high priority via syscall burn
        for (int i = 0; i < SYS_ITERS; i++) getpid();

        // signal parent: burn done
        char c = 1;
        write(ready[1][1], &c, 1);
        close(ready[1][1]);

        // wait for parent's go
        read(go[1][0], &c, 1);
        close(go[1][0]);

        // allocate under pressure
        char *mem = sbrklazy(NPAGES * PGSIZE);
        if (mem == SBRK_ERROR) { exit(1); }
        for (int i = 0; i < NPAGES; i++) mem[i * PGSIZE] = (char)(i + 1);
        for (int i = 0; i < NPAGES; i++) (void)mem[i * PGSIZE];

        struct result r;
        r.mlfq_level = getlevel();
        getvmstats(getpid(), &r.s);
        write(res[1][1], &r, sizeof(r));
        close(res[1][1]);
        exit(0);
    }
    close(ready[1][1]); close(go[1][0]); close(res[1][1]);

    // ---- Parent: wait for both burns, release simultaneously ----
    char c;
    read(ready[0][0], &c, 1); close(ready[0][0]);
    read(ready[1][0], &c, 1); close(ready[1][0]);

    c = 1;
    write(go[0][1], &c, 1); close(go[0][1]);
    write(go[1][1], &c, 1); close(go[1][1]);

    wait(0); wait(0);

    struct result r[2];
    read(res[0][0], &r[0], sizeof(r[0])); close(res[0][0]);
    read(res[1][0], &r[1], sizeof(r[1])); close(res[1][0]);

    printf("INFO: child 0 (CPU-bound)     -- level=%d faults=%d evicted=%d swapped_out=%d swapped_in=%d\n",
           r[0].mlfq_level, r[0].s.page_faults, r[0].s.pages_evicted,
           r[0].s.pages_swapped_out, r[0].s.pages_swapped_in);
    printf("INFO: child 1 (syscall-heavy) -- level=%d faults=%d evicted=%d swapped_out=%d swapped_in=%d\n",
           r[1].mlfq_level, r[1].s.page_faults, r[1].s.pages_evicted,
           r[1].s.pages_swapped_out, r[1].s.pages_swapped_in);

    if (r[0].mlfq_level > r[1].mlfq_level)
        printf("PASS: CPU-bound at level %d > syscall-heavy at level %d -- priority difference confirmed\n",
               r[0].mlfq_level, r[1].mlfq_level);
    else
        printf("NOTE: levels CPU=%d syscall=%d -- no priority gap (try increasing CPU_ITERS)\n",
               r[0].mlfq_level, r[1].mlfq_level);

    if (r[0].s.pages_evicted > r[1].s.pages_evicted)
        printf("PASS: CPU-bound child evicted more (%d) than syscall-heavy (%d) -- scheduler-aware eviction working\n",
               r[0].s.pages_evicted, r[1].s.pages_evicted);
    else if (r[0].s.pages_evicted == r[1].s.pages_evicted)
        printf("FAIL: equal evictions (%d == %d) -- clock not distinguishing priorities\n",
               r[0].s.pages_evicted, r[1].s.pages_evicted);
    else
        printf("FAIL: syscall-heavy child evicted MORE (%d > %d) -- wrong direction\n",
               r[1].s.pages_evicted, r[0].s.pages_evicted);

    exit(0);
}
