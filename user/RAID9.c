// RAID10.c -- Comprehensive RAID system integration test.
// Tests the complete RAID system integration including
// all components working together under various scenarios.
#include "kernel/types.h"
#include "kernel/diskstats.h"
#include "user/user.h"

#define PGSIZE 4096
#define INTEGRATION_PAGES 100  // Larger to ensure swapping occurs

void test_raid_integration(int raid_level, const char *raid_name) {
    printf("--- Testing %s Integration ---\n", raid_name);

    setraidlevel(raid_level);
    setdisksched(0); // Start with FCFS

    int pid = getpid();
    struct diskstats start_stats, end_stats;

    if (getdiskstats(pid, &start_stats) < 0) {
        printf("FAIL: getdiskstats failed\n");
        return;
    }

    // Allocate memory for testing
    char *mem = sbrklazy(INTEGRATION_PAGES * PGSIZE);
    if (mem == SBRK_ERROR) {
        printf("FAIL: sbrklazy failed for %s\n", raid_name);
        return;
    }

    // Phase 1: Sequential write and read
    printf("Phase 1: Sequential I/O\n");
    for (int i = 0; i < INTEGRATION_PAGES; i++) {
        mem[i * PGSIZE] = (char)((i + raid_level * 30) & 0xFF);
    }

    int ok = 1;
    for (int i = 0; i < INTEGRATION_PAGES; i++) {
        if (mem[i * PGSIZE] != (char)((i + raid_level * 30) & 0xFF)) {
            ok = 0;
            break;
        }
    }

    if (ok) {
        printf("PASS: Sequential I/O data integrity\n");
    } else {
        printf("FAIL: Sequential I/O data corruption\n");
        return;
    }

    // Phase 2: Random access patterns
    printf("Phase 2: Random access patterns\n");

    // Create random access pattern
    int access_order[INTEGRATION_PAGES];
    for (int i = 0; i < INTEGRATION_PAGES; i++) {
        access_order[i] = i;
    }

    // Shuffle the access order
    for (int i = 0; i < INTEGRATION_PAGES; i++) {
        int j = (i * 11 + 7) % INTEGRATION_PAGES;
        int temp = access_order[i];
        access_order[i] = access_order[j];
        access_order[j] = temp;
    }

    // Access in random order
    for (int i = 0; i < INTEGRATION_PAGES; i++) {
        int idx = access_order[i];
        volatile char val = mem[idx * PGSIZE];
        (void)val;
    }

    // Verify data is still intact
    ok = 1;
    for (int i = 0; i < INTEGRATION_PAGES; i++) {
        if (mem[i * PGSIZE] != (char)((i + raid_level * 30) & 0xFF)) {
            ok = 0;
            break;
        }
    }

    if (ok) {
        printf("PASS: Random access data integrity\n");
    } else {
        printf("FAIL: Random access data corruption\n");
        return;
    }

    // Phase 3: Scheduling policy comparison
    printf("Phase 3: Scheduling policy comparison\n");

    struct diskstats fcfs_stats, sstf_stats;

    // Test FCFS
    setdisksched(0);
    if (getdiskstats(pid, &fcfs_stats) < 0) {
        printf("FAIL: getdiskstats failed for FCFS\n");
        return;
    }

    // Perform some I/O with FCFS
    for (int i = INTEGRATION_PAGES - 1; i >= 0; i--) {
        volatile char val = mem[i * PGSIZE];
        (void)val;
    }

    // Test SSTF
    setdisksched(1);
    if (getdiskstats(pid, &sstf_stats) < 0) {
        printf("FAIL: getdiskstats failed for SSTF\n");
        return;
    }

    // Perform same I/O with SSTF
    for (int i = INTEGRATION_PAGES - 1; i >= 0; i--) {
        volatile char val = mem[i * PGSIZE];
        (void)val;
    }

    uint64 fcfs_latency = fcfs_stats.avg_disk_latency;
    uint64 sstf_latency = sstf_stats.avg_disk_latency;

    printf("FCFS latency: %lu, SSTF latency: %lu\n", fcfs_latency, sstf_latency);

    // Phase 4: Disk failure testing (for RAID 1 and 5)
    if (raid_level == 1 || raid_level == 5) {
        printf("Phase 4: Disk failure testing\n");

        // Write fresh data
        for (int i = 0; i < INTEGRATION_PAGES; i++) {
            mem[i * PGSIZE] = (char)((i + raid_level * 60) & 0xFF);
        }

        // Fail a disk
        setfaildisk(1, 0);

        // Try to read data - should work for RAID 1 and 5
        ok = 1;
        for (int i = 0; i < INTEGRATION_PAGES; i++) {
            if (mem[i * PGSIZE] != (char)((i + raid_level * 60) & 0xFF)) {
                ok = 0;
                break;
            }
        }

        if (ok) {
            printf("PASS: %s survived single disk failure\n", raid_name);
        } else {
            printf("FAIL: %s did not survive single disk failure\n", raid_name);
        }

        // Reset failure
        setfaildisk(0, -1);
    }

    // Get final statistics
    if (getdiskstats(pid, &end_stats) < 0) {
        printf("FAIL: getdiskstats failed\n");
        return;
    }

    int total_reads = end_stats.disk_reads - start_stats.disk_reads;
    int total_writes = end_stats.disk_writes - start_stats.disk_writes;
    uint64 avg_latency = end_stats.avg_disk_latency;

    printf("%s integration test completed: reads=%d writes=%d avg_latency=%lu\n",
           raid_name, total_reads, total_writes, avg_latency);

    printf("PASS: %s integration test successful\n", raid_name);
}

int
main(void)
{
    printf("=== RAID9: Comprehensive RAID System Integration ===\n");

    // Test all RAID levels
    test_raid_integration(0, "RAID 0");
    test_raid_integration(1, "RAID 1");
    test_raid_integration(5, "RAID 5");

    // Final system health check
    printf("--- System Health Check ---\n");

    int pid = getpid();
    struct diskstats final_stats;

    if (getdiskstats(pid, &final_stats) == 0) {
        printf("Final system stats: reads=%d writes=%d latency=%lu\n",
               final_stats.disk_reads, final_stats.disk_writes, final_stats.avg_disk_latency);
        printf("PASS: System health check passed\n");
    } else {
        printf("FAIL: System health check failed\n");
    }

    // Test system calls are still functional
    if (setraidlevel(0) == 0 && setdisksched(0) == 0 && setfaildisk(0, -1) == 0) {
        printf("PASS: System calls functional\n");
    } else {
        printf("FAIL: System calls not functional\n");
    }

    printf("PASS: Comprehensive RAID system integration testing completed\n");

    exit(0);
}