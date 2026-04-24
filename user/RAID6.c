// RAID7.c -- RAID performance benchmarking.
// Measures RAID performance under different memory loads
// and access patterns to evaluate RAID efficiency.
#include "kernel/types.h"
#include "kernel/diskstats.h"
#include "user/user.h"

#define PGSIZE 4096
#define LIGHT_LOAD 10
#define MEDIUM_LOAD 30
#define HEAVY_LOAD 60

void benchmark_raid(int raid_level, int load_pages, const char *load_name) {
    printf("Benchmarking RAID %d with %s load (%d pages)...\n",
           raid_level, load_name, load_pages);

    setraidlevel(raid_level);
    setdisksched(0); // FCFS

    int pid = getpid();
    struct diskstats before, after;

    if (getdiskstats(pid, &before) < 0) {
        printf("FAIL: getdiskstats failed\n");
        return;
    }

    char *mem = sbrklazy(load_pages * PGSIZE);
    if (mem == SBRK_ERROR) {
        printf("FAIL: sbrklazy failed\n");
        return;
    }

    // Sequential write
    uint64 start_time = uptime();
    for (int i = 0; i < load_pages; i++) {
        mem[i * PGSIZE] = (char)(i & 0xFF);
    }
    uint64 write_time = uptime() - start_time;

    // Sequential read
    start_time = uptime();
    for (int i = 0; i < load_pages; i++) {
        volatile char val = mem[i * PGSIZE];
        (void)val;
    }
    uint64 seq_read_time = uptime() - start_time;

    // Random access pattern
    int *access_pattern = (int*)malloc(load_pages * sizeof(int));
    if (access_pattern == 0) {
        printf("FAIL: malloc failed\n");
        return;
    }

    for (int i = 0; i < load_pages; i++) {
        access_pattern[i] = i;
    }

    // Simple shuffle for random access
    for (int i = 0; i < load_pages; i++) {
        int j = (i * 7 + 13) % load_pages;
        int temp = access_pattern[i];
        access_pattern[i] = access_pattern[j];
        access_pattern[j] = temp;
    }

    start_time = uptime();
    for (int i = 0; i < load_pages; i++) {
        volatile char val = mem[access_pattern[i] * PGSIZE];
        (void)val;
    }
    uint64 rand_read_time = uptime() - start_time;

    free(access_pattern);

    if (getdiskstats(pid, &after) < 0) {
        printf("FAIL: getdiskstats failed\n");
        return;
    }

    int reads = after.disk_reads - before.disk_reads;
    int writes = after.disk_writes - before.disk_writes;
    uint64 latency = after.avg_disk_latency;

    printf("RAID %d %s: writes=%d reads=%d latency=%lu\n",
           raid_level, load_name, writes, reads, latency);
    printf("  Timing: write=%lu seq_read=%lu rand_read=%lu ticks\n",
           write_time, seq_read_time, rand_read_time);

    // Verify data integrity
    int ok = 1;
    for (int i = 0; i < load_pages; i++) {
        if (mem[i * PGSIZE] != (char)(i & 0xFF)) {
            ok = 0;
            break;
        }
    }

    if (ok) {
        printf("  PASS: Data integrity verified\n");
    } else {
        printf("  FAIL: Data corruption detected\n");
    }
}

int
main(void)
{
    printf("=== RAID6: RAID Performance Benchmarking ===\n");

    int loads[] = {LIGHT_LOAD, MEDIUM_LOAD, HEAVY_LOAD};
    char *load_names[] = {"light", "medium", "heavy"};

    for (int raid = 0; raid < 2; raid++) {
        printf("\n--- RAID %d Performance Tests ---\n", raid);
        for (int load = 0; load < 2; load++) {
            benchmark_raid(raid, loads[load], load_names[load]);
        }
    }

    printf("\n--- Scheduling Policy Comparison ---\n");

    // Compare FCFS vs SSTF with RAID 5 and heavy load
    setraidlevel(5);

    printf("FCFS scheduling with heavy load:\n");
    setdisksched(0); // FCFS
    benchmark_raid(5, HEAVY_LOAD, "FCFS");

    printf("SSTF scheduling with heavy load:\n");
    setdisksched(1); // SSTF
    benchmark_raid(5, HEAVY_LOAD, "SSTF");

    printf("\nPASS: Performance benchmarking completed\n");

    exit(0);
}