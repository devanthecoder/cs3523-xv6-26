// RAID2.c -- RAID 1 disk failure test.
// Tests RAID 1 disk failure handling by failing one disk and
// verifying that reads still work using the mirrored disk.
#include "kernel/types.h"
#include "kernel/diskstats.h"
#include "user/user.h"

#define PGSIZE 4096
#define TEST_PAGES 200  // Much larger to force swapping

int
main(void)
{
    printf("=== RAID2: RAID 1 Disk Failure Test ===\n");

    int pid = getpid();
    struct diskstats before, after;

    // Set up RAID 1
    setraidlevel(1);
    setdisksched(0); // FCFS

    char *mem = sbrklazy(TEST_PAGES * PGSIZE);
    if (mem == SBRK_ERROR) {
        printf("FAIL: sbrklazy failed\n");
        exit(1);
    }

    // Write pattern to pages
    printf("Writing test data...\n");
    for (int i = 0; i < TEST_PAGES; i++) {
        mem[i * PGSIZE] = (char)(i + 1);
    }

    // Verify initial data
    int ok = 1;
    for (int i = 0; i < TEST_PAGES; i++) {
        if (mem[i * PGSIZE] != (char)(i + 1)) {
            printf("FAIL: Initial data corruption at page %d\n", i);
            ok = 0;
        }
    }

    if (!ok) {
        printf("FAIL: Initial data write failed\n");
        exit(1);
    }

    printf("PASS: Initial data written successfully\n");

    // Get stats before failure
    if (getdiskstats(pid, &before) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    // Fail disk 0
    printf("Failing disk 0...\n");
    setfaildisk(1, 0); // Fail disk 0

    // Read back and verify - should work using mirrored disk
    printf("Reading data after disk failure...\n");
    ok = 1;
    for (int i = 0; i < TEST_PAGES; i++) {
        if (mem[i * PGSIZE] != (char)(i + 1)) {
            printf("FAIL: Data corruption after disk failure at page %d\n", i);
            ok = 0;
        }
    }

    if (getdiskstats(pid, &after) < 0) {
        printf("FAIL: getdiskstats failed\n");
        exit(1);
    }

    printf("After failure: reads=%d writes=%d avg_latency=%lu\n",
           after.disk_reads - before.disk_reads,
           after.disk_writes - before.disk_writes,
           after.avg_disk_latency);

    if (ok) {
        printf("PASS: RAID 1 disk failure handling - data preserved\n");
    } else {
        printf("FAIL: RAID 1 disk failure handling - data lost\n");
    }

    // Test failing disk 2 (should be mirrored pair)
    printf("Testing disk 2 failure (mirrored pair)...\n");
    setfaildisk(1, 2); // Fail disk 2

    ok = 1;
    for (int i = 0; i < TEST_PAGES; i++) {
        if (mem[i * PGSIZE] != (char)(i + 1)) {
            printf("FAIL: Data corruption after disk 2 failure at page %d\n", i);
            ok = 0;
        }
    }

    if (ok) {
        printf("PASS: RAID 1 disk 2 failure handling\n");
    } else {
        printf("FAIL: RAID 1 disk 2 failure handling\n");
    }

    // Reset failure
    setfaildisk(0, -1);

    exit(0);
}