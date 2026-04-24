// RAID1.c -- Basic RAID functionality test.
// Tests RAID 0, RAID 1, and RAID 1 basic read/write operations.
// Measures disk statistics for each RAID level.
#include "kernel/types.h"
#include "kernel/diskstats.h"
#include "user/user.h"

#define PGSIZE 4096
#define TEST_PAGES 200  // Much larger to force swapping

int
main(void)
{
    printf("=== RAID 5: Basic RAID Functionality Test ===\n");

    int pid = getpid();
    struct diskstats before, after;

    // Test RAID 0
    printf("Testing RAID 5...\n");
    setraidlevel(5);
    setdisksched(0); // FCFS

    if (getdiskstats(pid, &before) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    char *mem = sbrklazy(TEST_PAGES * PGSIZE);
    if (mem == SBRK_ERROR) {
        printf("FAIL: sbrklazy failed\n");
        exit(1);
    }

    // Write pattern to pages
    for (int i = 0; i < TEST_PAGES; i++) {
        mem[i * PGSIZE] = (char)(i + 1);
    }

    // Read back and verify
    int ok = 1;
    for (int i = 0; i < TEST_PAGES; i++) {
        if (mem[i * PGSIZE] != (char)(i + 1)) {
            printf("FAIL: RAID 1 data corruption at page %d\n", i);
            ok = 0;
        }
    }

    if (getdiskstats(pid, &after) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    printf("RAID 5: reads=%d writes=%d avg_latency=%lu\n",
           after.disk_reads - before.disk_reads,
           after.disk_writes - before.disk_writes,
           after.avg_disk_latency);

    if (ok) printf("PASS: RAID 5 basic functionality\n");
    else printf("FAIL: RAID 5 data corruption\n");

    // // Test RAID 1
    // printf("Testing RAID 1...\n");
    // setraidlevel(1);
    // setdisksched(0); // FCFS

    // if (getdiskstats(pid, &before) < 0) {
    //     printf("FAIL: getdiskstats failed\n");
    //     exit(1);
    // }

    // // Write pattern to pages
    // for (int i = 0; i < TEST_PAGES; i++) {
    //     mem[i * PGSIZE] = (char)(i + 10);
    // }

    // // Read back and verify
    // ok = 1;
    // for (int i = 0; i < TEST_PAGES; i++) {
    //     if (mem[i * PGSIZE] != (char)(i + 10)) {
    //         printf("FAIL: RAID 1 data corruption at page %d\n", i);
    //         ok = 0;
    //     }
    // }

    // if (getdiskstats(pid, &after) < 0) {
    //     printf("FAIL: getdiskstats failed\n");
    //     exit(1);
    // }

    // printf("RAID 1: reads=%d writes=%d avg_latency=%lu\n",
    //        after.disk_reads - before.disk_reads,
    //        after.disk_writes - before.disk_writes,
    //        after.avg_disk_latency);

    // if (ok) printf("PASS: RAID 1 basic functionality\n");
    // else printf("FAIL: RAID 1 data corruption\n");

    // // Test RAID 1
    // printf("Testing RAID 1...\n");
    // setraidlevel(1);
    // setdisksched(0); // FCFS

    // if (getdiskstats(pid, &before) < 0) {
    //     printf("FAIL: getdiskstats failed\n");
    //     exit(1);
    // }

    // // Write pattern to pages
    // for (int i = 0; i < TEST_PAGES; i++) {
    //     mem[i * PGSIZE] = (char)(i + 20);
    // }

    // // Read back and verify
    // ok = 1;
    // for (int i = 0; i < TEST_PAGES; i++) {
    //     if (mem[i * PGSIZE] != (char)(i + 20)) {
    //         printf("FAIL: RAID 1 data corruption at page %d\n", i);
    //         ok = 0;
    //     }
    // }

    // if (getdiskstats(pid, &after) < 0) {
    //     printf("FAIL: getdiskstats failed\n");
    //     exit(1);
    // }

    // printf("RAID 1: reads=%d writes=%d avg_latency=%lu\n",
    //        after.disk_reads - before.disk_reads,
    //        after.disk_writes - before.disk_writes,
    //        after.avg_disk_latency);

    // if (ok) printf("PASS: RAID 1 basic functionality\n");
    // else printf("FAIL: RAID 1 data corruption\n");

    exit(0);
}