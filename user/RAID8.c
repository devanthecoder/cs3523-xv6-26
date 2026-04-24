// RAID9.c -- RAID edge cases and boundary testing.
// Tests RAID behavior at boundaries and edge cases
// to ensure robustness and correct error handling.
#include "kernel/types.h"
#include "kernel/diskstats.h"
#include "user/user.h"

#define PGSIZE 4096
#define MAX_PAGES 200  // Larger to ensure swapping occurs

int
main(void)
{
    printf("=== RAID8: RAID Edge Cases and Boundaries ===\n");

    int pid = getpid();

    // Test 1: Minimum memory allocation (1 page)
    printf("Test 1: Minimum allocation (1 page)\n");
    setraidlevel(5);
    setdisksched(0);

    char *min_mem = sbrklazy(1 * PGSIZE);
    if (min_mem == SBRK_ERROR) {
        printf("FAIL: sbrklazy failed for 1 page\n");
        exit(1);
    }

    min_mem[0] = 42;
    volatile char val = min_mem[0];
    if (val != 42) {
        printf("FAIL: Single page data corruption\n");
    } else {
        printf("PASS: Single page allocation works\n");
    }

    // Test 2: Large allocation approaching limits
    printf("Test 2: Large allocation boundary\n");
    char *large_mem = sbrklazy(MAX_PAGES * PGSIZE);
    if (large_mem == SBRK_ERROR) {
        printf("FAIL: sbrklazy failed for large allocation\n");
        exit(1);
    }

    // Write pattern to large allocation
    for (int i = 0; i < MAX_PAGES; i++) {
        large_mem[i * PGSIZE] = (char)(i & 0xFF);
    }

    // Verify large allocation
    int ok = 1;
    for (int i = 0; i < MAX_PAGES; i++) {
        if (large_mem[i * PGSIZE] != (char)(i & 0xFF)) {
            ok = 0;
            break;
        }
    }

    if (ok) {
        printf("PASS: Large allocation data integrity\n");
    } else {
        printf("FAIL: Large allocation data corruption\n");
    }

    // Test 3: RAID level boundaries
    printf("Test 3: RAID level boundary testing\n");

    // Test invalid RAID levels
    int invalid_levels[] = {-1, 3, 10};
    for (int i = 0; i < sizeof(invalid_levels)/sizeof(int); i++) {
        int result = setraidlevel(invalid_levels[i]);
        if (result == 0) {
            printf("NOTE: RAID level %d accepted (might be valid)\n", invalid_levels[i]);
        } else {
            printf("PASS: RAID level %d rejected as expected\n", invalid_levels[i]);
        }
    }

    // Test valid RAID levels
    int valid_levels[] = {0, 1, 5};
    for (int i = 0; i < sizeof(valid_levels)/sizeof(int); i++) {
        int result = setraidlevel(valid_levels[i]);
        if (result == 0) {
            printf("PASS: RAID level %d accepted\n", valid_levels[i]);
        } else {
            printf("FAIL: RAID level %d rejected unexpectedly\n", valid_levels[i]);
        }
    }

    // Test 4: Scheduling policy boundaries
    printf("Test 4: Scheduling policy boundary testing\n");

    int invalid_sched[] = {-1, 2, 10};
    for (int i = 0; i < sizeof(invalid_sched)/sizeof(int); i++) {
        int result = setdisksched(invalid_sched[i]);
        if (result == 0) {
            printf("NOTE: Scheduling policy %d accepted\n", invalid_sched[i]);
        } else {
            printf("PASS: Scheduling policy %d rejected\n", invalid_sched[i]);
        }
    }

    // Test 5: Disk failure boundary testing
    printf("Test 5: Disk failure boundary testing\n");

    // Test invalid disk numbers
    int invalid_disks[] = {-2, 10, 100};
    for (int i = 0; i < sizeof(invalid_disks)/sizeof(int); i++) {
        int result = setfaildisk(1, invalid_disks[i]);
        if (result == 0) {
            printf("NOTE: Disk %d failure accepted\n", invalid_disks[i]);
        } else {
            printf("PASS: Disk %d failure rejected\n", invalid_disks[i]);
        }
    }

    // Test 6: Multiple disk failures in RAID 5
    printf("Test 6: Multiple simultaneous disk failures\n");
    setraidlevel(5);

    char *test_mem = sbrklazy(9 * PGSIZE); // 3 stripes
    if (test_mem == SBRK_ERROR) {
        printf("FAIL: sbrklazy failed for test memory\n");
        exit(1);
    }

    // Write test data
    for (int i = 0; i < 9; i++) {
        test_mem[i * PGSIZE] = (char)(i + 1);
    }

    // Fail multiple disks at once
    setfaildisk(1, 0);
    setfaildisk(1, 1);
    setfaildisk(1, 2);

    // Check data - should be corrupted
    int corrupted = 0;
    for (int i = 0; i < 9; i++) {
        if (test_mem[i * PGSIZE] != (char)(i + 1)) {
            corrupted++;
        }
    }

    if (corrupted > 0) {
        printf("PASS: Multiple disk failures caused %d corruptions as expected\n", corrupted);
    } else {
        printf("FAIL: Multiple disk failures did not cause expected corruption\n");
    }

    // Test 7: Disk failure reset testing
    printf("Test 7: Disk failure reset testing\n");
    setfaildisk(0, -1); // Reset all

    // Write fresh data
    for (int i = 0; i < 9; i++) {
        test_mem[i * PGSIZE] = (char)(i + 100);
    }

    // Verify data is intact after reset
    ok = 1;
    for (int i = 0; i < 9; i++) {
        if (test_mem[i * PGSIZE] != (char)(i + 100)) {
            ok = 0;
            break;
        }
    }

    if (ok) {
        printf("PASS: Disk failure reset restored data integrity\n");
    } else {
        printf("FAIL: Disk failure reset did not restore data integrity\n");
    }

    // Test 8: Statistics boundary testing
    printf("Test 8: Disk statistics boundary testing\n");

    struct diskstats stats;
    int result = getdiskstats(pid, &stats);
    if (result == 0) {
        printf("PASS: getdiskstats succeeded\n");
        printf("Current stats: reads=%d writes=%d latency=%lu\n",
               stats.disk_reads, stats.disk_writes, stats.avg_disk_latency);
    } else {
        printf("FAIL: getdiskstats failed\n");
    }

    // Test with invalid PID
    result = getdiskstats(-1, &stats);
    if (result != 0) {
        printf("PASS: getdiskstats rejected invalid PID\n");
    } else {
        printf("NOTE: getdiskstats accepted invalid PID\n");
    }

    // Test 9: Memory access patterns at boundaries
    printf("Test 9: Memory access pattern boundaries\n");

    // Test accessing page boundaries
    char *boundary_mem = sbrklazy(3 * PGSIZE);
    if (boundary_mem == SBRK_ERROR) {
        printf("FAIL: sbrklazy failed for boundary test\n");
        exit(1);
    }

    // Write to start, middle, and end of pages
    boundary_mem[0] = 'A';
    boundary_mem[PGSIZE - 1] = 'B';
    boundary_mem[PGSIZE] = 'C';
    boundary_mem[2 * PGSIZE - 1] = 'D';
    boundary_mem[2 * PGSIZE] = 'E';

    // Read back
    if (boundary_mem[0] == 'A' &&
        boundary_mem[PGSIZE - 1] == 'B' &&
        boundary_mem[PGSIZE] == 'C' &&
        boundary_mem[2 * PGSIZE - 1] == 'D' &&
        boundary_mem[2 * PGSIZE] == 'E') {
        printf("PASS: Page boundary access works correctly\n");
    } else {
        printf("FAIL: Page boundary access failed\n");
    }

    printf("PASS: RAID edge cases and boundary testing completed\n");

    exit(0);
}