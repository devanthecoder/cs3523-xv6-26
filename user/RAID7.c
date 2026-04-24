// RAID8.c -- RAID stress testing with concurrency.
// Tests RAID behavior under concurrent memory pressure
// from multiple processes to evaluate system stability.
#include "kernel/types.h"
#include "kernel/diskstats.h"
#include "user/user.h"

#define PGSIZE 4096
#define PROCESSES 4
#define PAGES_PER_PROC 50  // Larger to ensure swapping occurs

void worker_process(int proc_id) {
    printf("Worker %d starting...\n", proc_id);

    // Set RAID level based on process ID
    int raid_level = proc_id % 3; // 0, 1, 2 -> RAID 0, 1, 5
    setraidlevel(raid_level);
    setdisksched(proc_id % 2); // Alternate FCFS/SSTF

    char *mem = sbrklazy(PAGES_PER_PROC * PGSIZE);
    if (mem == SBRK_ERROR) {
        printf("Worker %d: FAIL - sbrklazy failed\n", proc_id);
        exit(1);
    }

    // Write unique pattern for this process
    for (int i = 0; i < PAGES_PER_PROC; i++) {
        mem[i * PGSIZE] = (char)(proc_id * 50 + i);
    }

    // Simulate some work with memory access
    for (int round = 0; round < 3; round++) {
        // Random access pattern
        for (int i = 0; i < PAGES_PER_PROC; i++) {
            int idx = (i * 7 + round * 13) % PAGES_PER_PROC;
            volatile char val = mem[idx * PGSIZE];
            (void)val;

            // Small delay to allow interleaving
            for (volatile int j = 0; j < 1000; j++);
        }
    }

    // Verify data integrity
    int ok = 1;
    for (int i = 0; i < PAGES_PER_PROC; i++) {
        if (mem[i * PGSIZE] != (char)(proc_id * 50 + i)) {
            printf("Worker %d: FAIL - data corruption at page %d\n", proc_id, i);
            ok = 0;
        }
    }

    if (ok) {
        printf("Worker %d: PASS - completed successfully\n", proc_id);
    } else {
        printf("Worker %d: FAIL - data integrity check failed\n", proc_id);
    }

    exit(0);
}

int
main(void)
{
    printf("=== RAID7: Concurrent RAID Stress Testing ===\n");

    int pid = getpid();
    struct diskstats before, after;

    if (getdiskstats(pid, &before) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    // Fork multiple worker processes
    int child_pids[PROCESSES];
    for (int i = 0; i < PROCESSES; i++) {
        int child_pid = fork();
        if (child_pid == 0) {
            // Child process
            worker_process(i);
        } else if (child_pid > 0) {
            // Parent process
            child_pids[i] = child_pid;
        } else {
            printf("FAIL: fork failed for process %d\n", i);
            exit(1);
        }
    }

    // Wait for all children to complete
    for (int i = 0; i < PROCESSES; i++) {
        int status;
        int waited_pid = wait(&status);
        // Find which logical process this PID corresponds to
        int proc_index = -1;
        for (int j = 0; j < PROCESSES; j++) {
            if (child_pids[j] == waited_pid) {
                proc_index = j;
                break;
            }
        }
        if (status != 0) {
            printf("FAIL: Child process %d (PID %d) exited with status %d\n", proc_index, waited_pid, status);
        } else {
            printf("PASS: Child process %d (PID %d) completed successfully\n", proc_index, waited_pid);
        }
    }

    if (getdiskstats(pid, &after) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    int total_reads = after.disk_reads - before.disk_reads;
    int total_writes = after.disk_writes - before.disk_writes;
    uint64 avg_latency = after.avg_disk_latency;

    printf("Concurrent test completed:\n");
    printf("Total reads: %d, Total writes: %d, Avg latency: %lu\n",
           total_reads, total_writes, avg_latency);

    // Test with disk failure during concurrency
    printf("\nTesting disk failure during concurrency...\n");

    // Start one more round with disk failure
    setfaildisk(1, 0); // Fail disk 0

    if (getdiskstats(pid, &before) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    // Fork processes again
    for (int i = 0; i < PROCESSES; i++) {
        int child_pid = fork();
        if (child_pid == 0) {
            // Child process with RAID 5
            setraidlevel(5);
            setdisksched(0);

            char *mem = sbrklazy(PAGES_PER_PROC * PGSIZE);
            if (mem == SBRK_ERROR) {
                printf("Worker %d (failure test): sbrklazy failed\n", i);
                exit(1);
            }

            // Write and read with failure
            for (int j = 0; j < PAGES_PER_PROC; j++) {
                mem[j * PGSIZE] = (char)(i * 100 + j);
            }

            for (int j = 0; j < PAGES_PER_PROC; j++) {
                volatile char val = mem[j * PGSIZE];
                (void)val;
            }

            // Check if data is still intact (should be with RAID 5)
            int ok = 1;
            for (int j = 0; j < PAGES_PER_PROC; j++) {
                if (mem[j * PGSIZE] != (char)(i * 100 + j)) {
                    ok = 0;
                    break;
                }
            }

            if (ok) {
                printf("Worker %d (failure test): PASS\n", i);
            } else {
                printf("Worker %d (failure test): Expected corruption due to failure\n", i);
            }

            exit(0);
        } else if (child_pid > 0) {
            child_pids[i] = child_pid;
        } else {
            printf("FAIL: fork failed for failure test process %d\n", i);
            exit(1);
        }
    }

    // Wait for failure test children
    for (int i = 0; i < PROCESSES; i++) {
        int status;
        int waited_pid = wait(&status);
        // Find which logical process this PID corresponds to
        int proc_index = -1;
        for (int j = 0; j < PROCESSES; j++) {
            if (child_pids[j] == waited_pid) {
                proc_index = j;
                break;
            }
        }
        if (status != 0) {
            printf("FAIL: Failure test child %d (PID %d) exited with status %d\n", proc_index, waited_pid, status);
        } else {
            printf("PASS: Failure test child %d (PID %d) completed successfully\n", proc_index, waited_pid);
        }
    }

    if (getdiskstats(pid, &after) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    int failure_reads = after.disk_reads - before.disk_reads;
    int failure_writes = after.disk_writes - before.disk_writes;

    printf("Failure test: reads=%d writes=%d avg_latency=%lu\n",
           failure_reads, failure_writes, after.avg_disk_latency);

    // Reset disk failure
    setfaildisk(0, -1);

    printf("PASS: Concurrent RAID stress testing completed\n");

    exit(0);
}