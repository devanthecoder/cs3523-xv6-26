## CS3523 - Operating Systems
### Programming Assignment - 3: Scheduler-Aware Page Replacement in xv6

This README describes the implementation of a page replacement subsystem in xv6, including a global frame table, the Clock page replacement algorithm, a swap space mechanism, scheduler-aware eviction integrated with the SC-MLFQ scheduler from PA2, and per-process virtual memory statistics retrieved through a new system call.

---

#### Implemented Features

##### 1. Frame Table

A global frame table `frame_table[NFRAME]` (with `NFRAME = 64` defined in `kernel/param.h`) is maintained in `kernel/vm.c`. Each entry is a `struct frame` containing:
- `in_use`: whether the frame is currently occupied
- `curr_proc`: pointer to the process owning the page
- `va`: virtual address this frame is mapped to
- `ref_bit`: reference bit used by the Clock algorithm
- `pa`: physical address of the frame

All modifications to the frame table are performed under `frame_lock` to ensure mutual exclusion across concurrent accesses.

##### 2. Swap Space

A global swap table `swap_table[SWAPSIZE]` (with `SWAPSIZE = 1024` defined in `kernel/param.h`) is maintained in `kernel/vm.c`. Each `struct swapslot` stores:
- `in_use`: whether this slot is currently holding a swapped-out page
- `pagetable`: the page table of the owning process (used as a unique identifier)
- `va`: virtual address of the swapped-out page
- `data[PGSIZE]`: a `char` array wherein each element represents a byte of the page's data.

The `PTE_S` bit (bit 8, reserved for software use in RISC-V) is used to mark PTEs whose pages are currently in swap. When a page is swapped out, `PTE_V` is cleared and `PTE_S` is set. When it is swapped back in, `PTE_V` is set and `PTE_S` is cleared. All swap table accesses are performed under `swap_lock`.

##### 3. Modified Clock Page Replacement Algorithm

`evict()` in `kernel/vm.c` implements a modified Clock algorithm using a nested `while` loop structure:

1. An **outer `while (choice == 0)`** loop retries the entire clock sweep until a victim is found. A `no_of_loops` counter panics after 2 failed full revolutions to catch a genuinely exhausted frame table.
2. **First inner loop** (`while ref_bit == 1`): advances `clock_hand` circularly, clearing `ref_bit` to 0 on each frame that has `in_use == 1` and `curr_proc != NULL` (i.e., frames actually owned by a process). Frames with `curr_proc == NULL` (in-transit frames) are skipped — their `ref_bit` is left untouched. The loop stops at the first frame with `ref_bit == 0`, recording it as `start`.
3. **Second inner loop** (`while ref_bit == 0`, bounded by `clock_hand == start`): scans from `start` through all contiguous `ref_bit == 0` frames. Among all frames with `in_use == 1` and `curr_proc != NULL`, it selects the one whose `curr_proc->level` is highest (i.e., the lowest scheduling priority process). The loop terminates when it wraps back to `start` or hits a `ref_bit == 1` frame.
4. If no victim was found (e.g., all `ref_bit == 0` frames are in-transit with `curr_proc == NULL`), the outer loop re-enters, the first loop clears more `ref_bit` values, and the second loop tries again.
5. Once a victim is chosen, `swap_out()` is called, `choice->curr_proc` is set to `NULL` (marking it in-transit), and the frame is returned to the caller.

##### 4. Scheduler-Aware Eviction

Within the second pass of `evict()`, candidate frames are compared by `clock_hand->curr_proc->level`. Frames belonging to processes at a higher MLFQ level number (lower scheduling priority) are preferred as eviction victims. This ensures that interactive, syscall-heavy processes (which stay at lower MLFQ levels) retain their working sets longer while CPU-bound background processes lose frames first.

##### 5. New System Call: `getvmstats(int pid, struct vmstats *info)`

Retrieves virtual memory statistics for the process with the given PID. The kernel populates:

```c
struct vmstats {
    int page_faults;        // total page faults (lazy + swap-in)
    int pages_evicted;      // frames stolen from this process
    int pages_swapped_in;   // times a swapped-out page was restored
    int pages_swapped_out;  // times a page was written to swap
    int resident_pages;     // currently mapped physical frames
};
```

Returns `0` on success, `-1` if the PID is invalid. Implementation is in `kernel/proc.c` as `vmstat()`, following the same pattern as `mlfqstat()` from PA2.

---

#### Kernel Modifications

- **`kernel/vm.c`**: Global `frame_table[NFRAME]`, `swap_table[SWAPSIZE]`, and `clock_hand` declared. New functions `swap_out()`, `swap_in()`, `vmfault()`, `evict()`, `ismapped()` added. Existing functions `uvmalloc()`, `uvmunmap()`, `uvmcopy()` modified to integrate with the frame table and swap system.
- **`kernel/proc.h`**: `struct proc` extended with `page_faults`, `pages_evicted`, `pages_swapped_in`, `pages_swapped_out`, `resident_pages`.
- **`kernel/proc.c`**: `vmstat()` helper added (populates `struct vmstats` for a given PID, analogous to `mlfqstat()`); `freeproc()` zeroes all new VM fields; `allocproc()` initialises them to 0.
- **`kernel/trap.c`**: `usertrap()` extended to handle page faults — `scause` values 13 (load), 15 (store), and 12 (instruction) are caught and dispatched to `vmfault()`. The faulting virtual address is read from `r_stval()`.
- **`kernel/kalloc.c`**: `struct spinlock frame_lock, swap_lock` declared here and initialised in `kinit()`, since `kalloc.c` is the natural home for physical memory infrastructure locks.
- **`kernel/riscv.h`**: `PTE_S (1L << 8)` added — uses bit 8 of a PTE, which is reserved for software in the RISC-V Sv39 spec, to mark pages currently residing in swap space.
- **`kernel/defs.h`**: Forward declarations added for `struct frame`, `struct vmstats`; new function signatures added under `vm.c` (`ismapped`, `vmfault`, `evict`, `swap_out`, `swap_in`) and `proc.c` (`vmstat`).
- **`kernel/vminfo.h`**: New header containing `struct vmstats` definition, included by both kernel and user programs.
- **`kernel/param.h`**: `NFRAME = 64` and `SWAPSIZE = 1024` defined.
- **`kernel/syscall.h`**, **`kernel/syscall.c`**, **`user/user.h`**, **`user/usys.pl`**: Updated to register `getvmstats` as system call 30.

---

#### Design Decisions and Assumptions

1. **`in_use` vs `curr_proc` as the ownership bit**:
   After eviction, a frame is in a transitional state: `swap_out()` has been called and the old process's PTE has been invalidated, but the new process hasn't taken ownership yet. Setting `curr_proc = NULL` immediately prevents the Clock algorithm from double-evicting the same frame (since the second pass requires `curr_proc != NULL`). Keeping `in_use = 1` during this window prevents the free-frame scan (which checks `in_use == 0`) from claiming the frame before the evict caller does. This two-bit invariant eliminates the race condition without requiring the caller to hold `frame_lock` continuously across the `evict()` call.

2. **Lock ordering: `swap_lock` → `frame_lock`**:
   `swap_in()` acquires `swap_lock` first (to find the swap slot), then `frame_lock` (to claim a frame). `swap_out()` is always called from within `evict()`, which already holds `frame_lock`. To avoid deadlock, `swap_out()` acquires `swap_lock` internally after `frame_lock` is already held. This is consistent with the ordering because `swap_in()` releases `swap_lock` before acquiring `frame_lock` (see `swap_in` at the point it calls `evict()`). Both paths therefore never hold both locks simultaneously in conflicting order.

3. **`PTE_S` bit for swap state**:
   RISC-V Sv39 PTEs have two bits (8 and 9) reserved for software use. Bit 8 is repurposed as `PTE_S` to mark a PTE whose page has been evicted to swap. When a page is swapped out, `PTE_V` is cleared (so any hardware access will fault) and `PTE_S` is set (so the fault handler knows to call `swap_in()` rather than allocate a fresh page). This avoids needing a separate data structure to track which virtual addresses are currently in swap.

4. **Global `clock_hand` for persistent clock state**:
   The clock hand is a global `struct frame *clock_hand` initialized to `frame_table`. This persists across calls to `evict()` so the algorithm resumes scanning from where it last stopped, rather than restarting from frame 0 every time. This is critical for correct clock behaviour: if the hand reset every eviction, recently allocated frames would never get a second chance and the algorithm would degenerate.

5. **Outer `while(choice == 0)` loop for robustness**:
   The second inner loop of the Clock exits early if it hits a `ref_bit == 1` frame before completing a full revolution. In some states (many in-transit frames, all owned frames recently accessed), a single pass may find no eligible victim. The outer loop retries by re-running the first pass (clearing more `ref_bit`s) until a victim is found. A `no_of_loops` counter limits retries to 2 full cycles before panicking, preventing an infinite loop in a genuinely exhausted frame table.

6. **`uvmcopy()` handles swapped pages**:
   During `fork()`, if the parent has pages in swap, `uvmcopy()` detects `PTE_S` and calls `swap_in()` to bring the page back into memory before copying it to the child. This ensures the child always starts with a complete, in-memory copy of the parent's address space.

7. **Swap implemented in kernel memory, not on disk**:
   As permitted by the assignment, swap space is a statically allocated in-kernel array. No file system or virtio disk operations are involved. This keeps swap fast but limits total swappable pages to `SWAPSIZE = 1024`.


---

#### Experimental Results

Six test programs were written in `user/vmswap{1-6}.c` to validate the implementation. Each test prints `PASS: ...` or `FAIL: ...` per check.

##### Test 1: vmswap1.c — Basic Lazy Allocation and Page Fault Tracking

Verifies that `sbrklazy()` does not immediately allocate frames, that first accesses trigger page faults, and that `page_faults` and `resident_pages` statistics are updated correctly.

```
=== vmswap1: Basic Lazy Alloc & Page Fault ===
PASS: all 20 pages readable after lazy page faults
PASS: page_faults increased by at least 20 (got +20)
PASS: resident_pages increased by 20
INFO: faults=20 evicted=0 swapped_out=0 swapped_in=0 resident=25
```

**Analysis**: All 20 lazily allocated pages triggered individual page faults on first access, confirming demand-paging behaviour. The `page_faults` counter incremented by exactly 20 (one per page) and `resident_pages` grew to 25 (the 20 new heap pages plus the process's existing code/stack pages). No eviction or swapping occurred since 25 resident pages is well within `NFRAME = 64`, confirming that `vmfault()` correctly allocates and maps pages without touching the swap path.

---

##### Test 2: vmswap2.c — Data Integrity Across Swap-Out and Swap-In

Allocates 80 pages (exceeding `NFRAME = 64`), writes a per-page XOR pattern, then reads all pages back. This forces the Clock algorithm to evict early pages when later ones are accessed, and then swap them back in during the read phase.

```
=== vmswap2: Swap-Out / Swap-In Data Integrity ===
PASS: all 80 pages intact after eviction/swap-in
PASS: pages_swapped_out increased by 104
PASS: pages_swapped_in increased by 83
INFO: faults=163 evicted=104 swapped_out=104 swapped_in=83 resident=64
```

**Analysis**: With only 64 frames available system-wide, allocating 80 pages guaranteed eviction. 104 swap-outs occurred (pages evicted multiple times as the clock hand cycled through) and 83 swap-ins were needed to read back all 80 pages. The frame table reached saturation at `resident=64`. All 80 pages survived the swap-out and swap-in cycle with correct data, confirming that `swap_out()` faithfully copies page contents and `swap_in()` correctly restores them and updates the PTE. The 163 total faults reflect both the 80 initial allocation faults and the 83 swap-in faults.

---

##### Test 3: vmswap3.c — Full-Page Byte-Level Integrity

Writes a unique value to every byte of every page across 70 pages (not just the first byte), then reads all bytes back, including via a non-sequential stride access pattern. This catches partial-page corruption that a first-byte-only check would miss.

```
=== vmswap3: Full-Page Byte-Level Integrity ===
PASS: all 70 pages x 4096 bytes intact (full-page check)
PASS: stride (non-sequential) access preserves all page data
INFO: faults=86 evicted=27 swapped_out=27 swapped_in=16 resident=64
```

**Analysis**: Writing 70 × 4096 = 286,720 distinct bytes and verifying each one confirms that `memmove` in `swap_out()` and `swap_in()` copies the full page contents faithfully with no off-by-one or size errors. 27 pages were evicted to accommodate the 70-page working set within 64 frames, and 16 swap-ins were needed during the stride read-back pass. The stride access (every 7th page, non-sequential) exercises scattered swap-in where the kernel must restore individual pages in arbitrary order. All 286,720 bytes matched their expected values.

---

##### Test 4: vmswap4.c — Fork with Pages in Swap

The parent allocates 70 pages and writes a pattern (pushing earlier pages into swap), then calls `fork()`. The child verifies it can read back all pages correctly. The parent then verifies its own copy was not corrupted by the child's subsequent mutation of its pages.

```
=== vmswap4: Fork with Pages in Swap ===
PASS: child sees correct data for all 70 pages post-fork
PASS: child mutation complete (COW would be tested separately)
INFO: child -- faults=145 evicted=156 swapped_out=156 swapped_in=145 resident=64
PASS: parent data intact after child mutation
PASS: child exited successfully
INFO: parent -- faults=210 evicted=151 swapped_out=151 swapped_in=140 resident=64
```

**Analysis**: `uvmcopy()` correctly handled PTEs marked with `PTE_S` by calling `swap_in()` before copying each swapped page to the child's address space. The child received a valid, independent copy of all 70 pages. The very high eviction and swap counts (child: 156 swapped_out/145 swapped_in; parent: 151/140) are expected: after `fork()`, both processes each need up to 70 resident pages simultaneously, but only 64 frames exist system-wide, so both processes are constantly evicting and restoring each other's pages. Despite this extreme pressure, both address spaces remained fully intact and independent: the child's mutation of its pages did not corrupt the parent's copy, confirming correct deep-copy semantics from `uvmcopy()`.

---

##### Test 5: vmswap5.c — Multi-Process Memory Pressure

Spawns 3 children, each lazily allocating 35 pages (105 total across children, well above `NFRAME = 64`). All children run concurrently, competing for physical frames, and each independently verifies its own data after the pressure subsides.

```
=== vmswap5: Multi-Process Memory Pressure ===
PASS: child 0 all 35 pages correct under pressure
INFO: child 0 -- faults=72 evicted=58 swapped_out=58 swapped_in=37 resident=19
PASS: child 1 all 35 pages correct under pressure
INFO: child 1 -- faults=73 evicted=56 swapped_out=56 swapped_in=38 resident=22
PASS: child 2 all 35 pages correct under pressure
INFO: child 2 -- faults=74 evicted=53 swapped_out=53 swapped_in=39 resident=26
PASS: all 3 children survived memory pressure with correct data
INFO: parent -- faults=2 evicted=5 swapped_out=5 swapped_in=2 resident=2
```

**Analysis**: With three children collectively requiring 105 frames but only 64 available system-wide, heavy concurrent eviction is unavoidable. Each child accumulated ~70+ page faults (35 initial allocation faults plus ~37 swap-in faults as rival processes stole their frames) and ~55 evictions each. Despite this, all three children read back their 35 pages without any corruption, demonstrating that the locking discipline (the `in_use`/`curr_proc` in-transit state, consistent `swap_lock → frame_lock` ordering, and the full-revolution Clock second pass) correctly serialises concurrent evictions. The parent's small eviction count (5) reflects the Clock algorithm's scheduler-aware policy: since all children are CPU-bound and demote to lower MLFQ levels, the parent (which stays at a higher priority) had a few of its frames stolen early but was largely protected.

---

##### Test 6: vmswap6.c — Scheduler-Aware Eviction

Child 0 burns CPU (`for` loop, no syscalls) to drop in scheduling priority (reaching a higher MLFQ level). Child 1 calls `getpid()` 25,000 times to stay at the highest priority (level 0). Both signal readiness via a pipe, the parent releases them simultaneously, and both lazily allocate 40 pages each (80 total, well above the 64-frame limit) under mutual pressure. The Clock algorithm should preferentially evict child 0's frames.

```
=== vmswap6: Scheduler-Aware Eviction ===
INFO: child 0 (CPU-bound)     -- level=1 faults=45 evicted=27 swapped_out=27 swapped_in=5
INFO: child 1 (syscall-heavy) -- level=0 faults=40 evicted=11 swapped_out=11 swapped_in=0
PASS: CPU-bound at level 1 > syscall-heavy at level 0 -- priority difference confirmed
PASS: CPU-bound child evicted more (27) than syscall-heavy (11) -- scheduler-aware eviction working
```

**Analysis**: Child 0 demoted to level 1 through CPU burning and had 27 frames stolen from it, which is more than double child 1's 11 evictions. Child 1 successfully remained at level 0 thanks to its high syscall rate satisfying the SC-MLFQ demotion-prevention rule. During the concurrent allocation phase, the Clock's second pass compared `curr_proc->level` across eligible frames and consistently picked child 0's frames (level 1) over child 1's (level 0). The fault and swap-in counts further validate this behavior: child 1 experienced exactly 40 faults (one for each of its 40 allocated pages) and 0 swap-ins, meaning its working set was heavily protected. Conversely, child 0 suffered 45 faults and 5 swap-ins, reflecting the penalty of having its pages aggressively targeted by the evictor. This confirms that the Clock algorithm correctly integrates with the SC-MLFQ scheduler, successfully fulfilling the requirement to preferentially evict the pages of lower-priority processes during memory pressure.

---

#### Summary

| Feature | Test | Result |
|---|---|---|
| Lazy allocation + page fault tracking | vmswap1 | PASS |
| Swap-out / swap-in data integrity | vmswap2 | PASS |
| Full-page (byte-level) integrity | vmswap3 | PASS |
| `fork()` with pages in swap (`uvmcopy`) | vmswap4 | PASS |
| Concurrent multi-process pressure | vmswap5 | PASS |
| Scheduler-aware eviction (MLFQ priority) | vmswap6 | PASS |

All six test programs pass cleanly. The implementation correctly handles lazy allocation, demand paging, page eviction via the Clock algorithm, swap-out and swap-in with full data integrity, `fork()` across swapped pages, concurrent multi-process memory pressure, and scheduler-aware eviction preferring lower-priority processes — with correct locking discipline throughout.