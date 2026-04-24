// RAID3.c -- RAID 5 disk failure test.
// Tests RAID 5 disk failure handling by failing one disk and
// verifying that reads reconstruct data using parity.
#include "kernel/types.h"
#include "kernel/diskstats.h"
#include "user/user.h"

#define PGSIZE 4096
#define TEST_PAGES 198  // Multiple of 3 for clean stripe testing, large enough to force swapping

int
main(void)
{
    printf("=== RAID3: RAID 5 Disk Failure Test ===\n");

    int pid = getpid();
    struct diskstats before, after;

    // Set up RAID 5
    setraidlevel(5);
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

    // Read back and verify - should reconstruct using parity
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
        printf("PASS: RAID 5 disk failure handling - data reconstructed\n");
    } else {
        printf("FAIL: RAID 5 disk failure handling - data lost\n");
    }

    // Test failing disk 1
    printf("Testing disk 1 failure...\n");
    setfaildisk(1, 1); // Fail disk 1

    ok = 1;
    for (int i = 0; i < TEST_PAGES; i++) {
        if (mem[i * PGSIZE] != (char)(i + 1)) {
            printf("FAIL: Data corruption after disk 1 failure at page %d\n", i);
            ok = 0;
        }
    }

    if (ok) {
        printf("PASS: RAID 5 disk 1 failure handling\n");
    } else {
        printf("FAIL: RAID 5 disk 1 failure handling\n");
    }

    // Test failing disk 2
    printf("Testing disk 2 failure...\n");
    setfaildisk(1, 2); // Fail disk 2

    ok = 1;
    for (int i = 0; i < TEST_PAGES; i++) {
        if (mem[i * PGSIZE] != (char)(i + 1)) {
            printf("FAIL: Data corruption after disk 2 failure at page %d\n", i);
            ok = 0;
        }
    }

    if (ok) {
        printf("PASS: RAID 5 disk 2 failure handling\n");
    } else {
        printf("FAIL: RAID 5 disk 2 failure handling\n");
    }

    // Test failing parity disk (disk 3 in first stripe)
    printf("Testing parity disk failure...\n");
    setfaildisk(1, 3); // Fail disk 3 (parity in first stripe)

    ok = 1;
    for (int i = 0; i < TEST_PAGES; i++) {
        if (mem[i * PGSIZE] != (char)(i + 1)) {
            printf("FAIL: Data corruption after parity disk failure at page %d\n", i);
            ok = 0;
        }
    }

    if (ok) {
        printf("PASS: RAID 5 parity disk failure handling\n");
    } else {
        printf("FAIL: RAID 5 parity disk failure handling\n");
    }

    // Reset failure
    setfaildisk(0, -1);

    exit(0);
}