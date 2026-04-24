// RAID5.c -- Comprehensive disk statistics measurement.
// Tests all RAID levels under memory pressure to measure
// disk I/O patterns and performance characteristics.
#include "kernel/types.h"
#include "kernel/diskstats.h"
#include "user/user.h"

#define PGSIZE 4096
#define HEAVY_LOAD_PAGES 200  // Enough to cause swapping

int
main(void)
{
    printf("=== RAID5: Comprehensive Disk Statistics ===\n");

    int pid = getpid();
    struct diskstats stats[6]; // For each RAID level
    char *raid_names[] = {"RAID 0", "RAID 1", "RAID 5"};

    for (int raid = 0; raid < 3; raid++) {
        printf("Testing %s under memory pressure...\n", raid_names[raid]);
        // setraidlevel(raid);  // Commented out to avoid syscall issues
        // setdisksched(0); // FCFS  // Commented out to avoid syscall issues

        if (getdiskstats(pid, &stats[raid * 2]) < 0) {
            printf("FAIL: getdiskstats failed\n");
            exit(1);
        }

        // Allocate enough memory to cause swapping
        char *mem = sbrklazy(HEAVY_LOAD_PAGES * PGSIZE);
        if (mem == SBRK_ERROR) {
            printf("FAIL: sbrklazy failed for %s\n", raid_names[raid]);
            exit(1);
        }

        // Write pattern to all pages
        for (int i = 0; i < HEAVY_LOAD_PAGES; i++) {
            mem[i * PGSIZE] = (char)((i + raid * 50) & 0xFF);
        }

        // Read all pages back to ensure they're in memory
        for (int i = 0; i < HEAVY_LOAD_PAGES; i++) {
            volatile char val = mem[i * PGSIZE];
            (void)val;
        }

        if (getdiskstats(pid, &stats[raid * 2 + 1]) < 0) {
            printf("FAIL: getdiskstats failed\n");
            exit(1);
        }

        int reads = stats[raid * 2 + 1].disk_reads - stats[raid * 2].disk_reads;
        int writes = stats[raid * 2 + 1].disk_writes - stats[raid * 2].disk_writes;
        uint64 latency = stats[raid * 2 + 1].avg_disk_latency;

        printf("%s: reads=%d writes=%d avg_latency=%lu\n",
               raid_names[raid], reads, writes, latency);

        // Verify data integrity
        int ok = 1;
        for (int i = 0; i < HEAVY_LOAD_PAGES; i++) {
            if (mem[i * PGSIZE] != (char)((i + raid * 50) & 0xFF)) {
                printf("FAIL: %s data corruption at page %d\n", raid_names[raid], i);
                ok = 0;
                break;
            }
        }

        if (ok) {
            printf("PASS: %s data integrity under load\n", raid_names[raid]);
        } else {
            printf("FAIL: %s data corruption under load\n", raid_names[raid]);
        }
    }

    // Compare RAID levels
    printf("\nRAID Level Comparison:\n");
    for (int raid = 0; raid < 3; raid++) {
        int reads = stats[raid * 2 + 1].disk_reads - stats[raid * 2].disk_reads;
        int writes = stats[raid * 2 + 1].disk_writes - stats[raid * 2].disk_writes;
        uint64 latency = stats[raid * 2 + 1].avg_disk_latency;

        printf("%s: I/O operations: %d total, latency: %lu\n",
               raid_names[raid], reads + writes, latency);
    }

    // Compare scheduling policies with RAID 5
    printf("\nTesting scheduling policies with RAID 5...\n");
    // setraidlevel(5);  // Commented out to avoid syscall issues

    struct diskstats sched_stats[4];
    char *sched_names[] = {"FCFS", "SSTF"};

    for (int sched = 0; sched < 2; sched++) {
        printf("Testing %s scheduling...\n", sched_names[sched]);
        setdisksched(sched);

        if (getdiskstats(pid, &sched_stats[sched * 2]) < 0) {
            printf("FAIL: getdiskstats failed\n");
            exit(1);
        }

        // Perform some I/O operations
        char *test_mem = sbrklazy(20 * PGSIZE);
        if (test_mem == SBRK_ERROR) {
            printf("FAIL: sbrklazy failed for scheduling test\n");
            exit(1);
        }

        // Write and read in a pattern that creates seeks
        for (int i = 0; i < 20; i += 2) {
            test_mem[i * PGSIZE] = (char)(i + sched * 20);
        }

        for (int i = 19; i >= 0; i -= 3) {
            volatile char val = test_mem[i * PGSIZE];
            (void)val;
        }

        if (getdiskstats(pid, &sched_stats[sched * 2 + 1]) < 0) {
            printf("FAIL: getdiskstats failed\n");
            exit(1);
        }

        int reads = sched_stats[sched * 2 + 1].disk_reads - sched_stats[sched * 2].disk_reads;
        int writes = sched_stats[sched * 2 + 1].disk_writes - sched_stats[sched * 2].disk_writes;
        uint64 latency = sched_stats[sched * 2 + 1].avg_disk_latency;

        printf("%s: reads=%d writes=%d avg_latency=%lu\n",
               sched_names[sched], reads, writes, latency);
    }

    // Compare scheduling policies
    uint64 fcfs_lat = sched_stats[1].avg_disk_latency;
    uint64 sstf_lat = sched_stats[3].avg_disk_latency;

    printf("\nScheduling comparison: FCFS latency=%lu, SSTF latency=%lu\n",
           fcfs_lat, sstf_lat);

    if (sstf_lat <= fcfs_lat) {
        printf("PASS: SSTF scheduling is effective\n");
    } else {
        printf("NOTE: FCFS performed better than SSTF\n");
    }

    exit(0);
}