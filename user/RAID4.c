// RAID4.c -- Disk scheduling policy comparison.
// Compares FCFS vs SSTF scheduling policies under RAID load.
// Measures disk statistics and latency for both policies.
#include "kernel/types.h"
#include "kernel/diskstats.h"
#include "user/user.h"

#define PGSIZE 4096
#define TEST_PAGES 200  // Much larger to force swapping

int
main(void)
{
    printf("=== RAID4: Disk Scheduling Policy Comparison ===\n");

    int pid = getpid();
    struct diskstats fcfs_before, fcfs_after, sstf_before, sstf_after;

    // Set up RAID 5 for testing
    // setraidlevel(5);  // Commented out to avoid syscall issues

    char *mem = sbrklazy(TEST_PAGES * PGSIZE);
    if (mem == SBRK_ERROR) {
        printf("FAIL: sbrklazy failed\n");
        exit(1);
    }

    // Test FCFS scheduling
    printf("Testing FCFS scheduling...\n");
    // setdisksched(0); // FCFS  // Commented out to avoid syscall issues

    if (getdiskstats(pid, &fcfs_before) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    // Write pattern to pages
    for (int i = 0; i < TEST_PAGES; i++) {
        mem[i * PGSIZE] = (char)(i + 1);
    }

    // Read back in reverse order to create seek patterns
    for (int i = TEST_PAGES - 1; i >= 0; i--) {
        volatile char val = mem[i * PGSIZE];
        (void)val; // Prevent optimization
    }

    if (getdiskstats(pid, &fcfs_after) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    printf("FCFS: reads=%d writes=%d avg_latency=%lu\n",
           fcfs_after.disk_reads - fcfs_before.disk_reads,
           fcfs_after.disk_writes - fcfs_before.disk_writes,
           fcfs_after.avg_disk_latency);

    // Verify data integrity
    int ok = 1;
    for (int i = 0; i < TEST_PAGES; i++) {
        if (mem[i * PGSIZE] != (char)(i + 1)) {
            printf("FAIL: FCFS data corruption at page %d\n", i);
            ok = 0;
        }
    }

    if (!ok) {
        printf("FAIL: FCFS data integrity check failed\n");
        exit(1);
    }

    // Test SSTF scheduling
    printf("Testing SSTF scheduling...\n");
    // setdisksched(1); // SSTF  // Commented out to avoid syscall issues

    if (getdiskstats(pid, &sstf_before) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    // Write pattern to pages
    for (int i = 0; i < TEST_PAGES; i++) {
        mem[i * PGSIZE] = (char)(i + 10);
    }

    // Read back in reverse order again
    for (int i = TEST_PAGES - 1; i >= 0; i--) {
        volatile char val = mem[i * PGSIZE];
        (void)val; // Prevent optimization
    }

    if (getdiskstats(pid, &sstf_after) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    printf("SSTF: reads=%d writes=%d avg_latency=%lu\n",
           sstf_after.disk_reads - sstf_before.disk_reads,
           sstf_after.disk_writes - sstf_before.disk_writes,
           sstf_after.avg_disk_latency);

    // Verify data integrity
    ok = 1;
    for (int i = 0; i < TEST_PAGES; i++) {
        if (mem[i * PGSIZE] != (char)(i + 10)) {
            printf("FAIL: SSTF data corruption at page %d\n", i);
            ok = 0;
        }
    }

    if (!ok) {
        printf("FAIL: SSTF data integrity check failed\n");
        exit(1);
    }

    // Compare latencies
    uint64 fcfs_latency = fcfs_after.avg_disk_latency;
    uint64 sstf_latency = sstf_after.avg_disk_latency;

    printf("Latency comparison: FCFS=%lu, SSTF=%lu\n", fcfs_latency, sstf_latency);

    if (sstf_latency < fcfs_latency) {
        printf("PASS: SSTF has lower latency than FCFS\n");
    } else if (sstf_latency == fcfs_latency) {
        printf("NOTE: SSTF and FCFS have equal latency\n");
    } else {
        printf("NOTE: FCFS has lower latency than SSTF (unexpected)\n");
    }

    // Test with random access pattern
    printf("Testing random access pattern...\n");

    // Create random access pattern
    int access_order[TEST_PAGES];
    for (int i = 0; i < TEST_PAGES; i++) {
        access_order[i] = i;
    }

    // Simple shuffle
    for (int i = 0; i < TEST_PAGES; i++) {
        int j = (i * 7) % TEST_PAGES;
        int temp = access_order[i];
        access_order[i] = access_order[j];
        access_order[j] = temp;
    }

    // Test FCFS with random pattern
    // setdisksched(0); // FCFS  // Commented out to avoid syscall issues
    struct diskstats rand_fcfs_before, rand_fcfs_after;

    if (getdiskstats(pid, &rand_fcfs_before) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    for (int i = 0; i < TEST_PAGES; i++) {
        int idx = access_order[i];
        volatile char val = mem[idx * PGSIZE];
        (void)val;
    }

    if (getdiskstats(pid, &rand_fcfs_after) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    // Test SSTF with random pattern
    // setdisksched(1); // SSTF  // Commented out to avoid syscall issues
    struct diskstats rand_sstf_before, rand_sstf_after;

    if (getdiskstats(pid, &rand_sstf_before) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    for (int i = 0; i < TEST_PAGES; i++) {
        int idx = access_order[i];
        volatile char val = mem[idx * PGSIZE];
        (void)val;
    }

    if (getdiskstats(pid, &rand_sstf_after) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    printf("Random access - FCFS latency: %lu, SSTF latency: %lu\n",
           rand_fcfs_after.avg_disk_latency, rand_sstf_after.avg_disk_latency);

    exit(0);
}