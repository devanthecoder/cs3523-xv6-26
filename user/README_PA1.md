## CS3523 - Operating Systems
### Programming Assignment - 1

This README intends to describe how a system call is implemented in xv6, the various system calls implemented as per the given Assignment's objective and the design decisions undertaken to achieve this goal.

#### Implementing a System Call in xv6

The implementation of a system call in xv6 requires making modifications to 5 files, 3 in the kernel folder and 2 in the user folder. The relevant files are as follows:

##### Kernel Files
1.  `kernel/syscall.h`: When adding a new system call, we define a new macro with the naming format `SYS_{syscall_name}` and set its value (ID) to the number just after the last syscall number. This ID serves as the number that maps to the newly defined system call. 
2.  `kernel/syscall.c`: We declare our system call having the naming format `sys_{syscall_name}` with the `extern` keyword and add it to the array of system calls `syscalls[]` so that upon compilation, system calls defined in `kernel/sysproc.c` get imported over into `kernel/syscall.c`.
3.  `kernel/sysproc.c`: This file consists of all the system call definition that do not involve I/O. Here, we define the actual content of our new system call, including the inputs it takes, the return value, any outputs, etc.

##### User Files
1.  `user/user.h`: This file contains the declaration of system calls to be directly called by the user. The system calls defined in Kernel are mapped to these system calls by their unique ID. Here, we declare the new system call that maps from the corresponding system call defined in Kernel.
2.  `user/usys.pl`: This file generates the `user/usys.S` file that initiates a trap when the user calls a system call, temporarily switching over control to the OS so that it can safely perform the system call. In `user/usys.pl`, we add the line `entry("{new_syscall_name}")` to add it to the list of existing system calls that require a trap to work.

#### Implemented System Calls and Attributes
Given below are the system calls implemented in accordance with the objective of the Assignment.

##### Part A: Warm-Up System Calls
1.  ###### `hello()`
    1.  What it does: A simple system call that prints `"Hello from the kernel!"` to the shell and returns `0`.
    2.  How it works: `hello()` calls `printf()` function and prints `"Hello from the kernel!"` to the shell.
2.  ###### `getpid2()`
    1.  What it does: Returns the calling process's PID.
    2.  How it works: `getpid2()` accesses the current running process by calling `myproc()` and returning its `pid` attribute, i.e. `myproc()->pid`.
##### Part B: Process Relationships
1.  ###### `getppid()`
    1.  What it does: Returns the PID of the calling process's parent and -1 otherwise if parent doesn't exist for the process (initproc).
    2.  How it works: As a reference to the parent process exists in `struct proc`, all that is required is calling `pid` of `parent` attribute contained in `myproc()`, i.e., `myproc()->parent->pid`.
2.  ###### `getnumchild()` 
    1.  What it does: Returns the number of existing, non-ZOMBIE child processes of the calling process.
    2.  How it works: We access the process table `proc` and iterate through each process contained in it. We maintain a counter variable `num` and increment it everytime we encounter a non-ZOMBIE process whose parent is calling process, i.e. `myproc()`. So, our condition for incrementation is `p->parent == myproc()` and then `p->state != ZOMBIE`. All this is done while ensuring proper locking for both the process table and the processes is maintained when accessing their contents.
##### Part C: System Call Accounting
1.  ###### Adding a Per Process System Call Counter:
    1.  What it does: To the `struct proc`, a `syscallcount` attribute is added that contains the number of system calls called by that particular process throughout its runtime.
    2.  How it works: When a process is first loaded into the system by `allocproc()` function in `kernel/proc.c`, the syscallcount is set to `0`. Then in `kernel/syscall.c`, the `syscall()` function is called whenever a system call is invoked by the user. So there, after setting necessary locks, we increment `syscallcount`. When `freeproc()` is called in `kernel/proc.c`, every numerical attribute is set to `0`, so we do the same for `syscallcount`. Like this, we have an attribute that contains the number of system calls called by any process at any given time.
2.  ###### `getsyscount()`:
    1.  What it does: Returns the number of system calls the calling process has invoked so far (the counting is done per-process and includes this `getsyscount()` call).
    2.  How it works: As explained previously, we have the `syscallcount` attribute that contains the number of system calls invoked by that process at that moment. So all we have to do is return the `syscallcount` attribute of `myproc()`, i.e. return `myproc()->syscallcount`.
3.  ###### `getchildsyscount(int pid)`:
    1.  What it does: Returns the number of system calls invoked by the child having PID `pid` if child exists and `-1` otherwise.
    2.  How it works: Like in `getnumchild()`, we access the process table `proc` and iterate through it till we reach a process that is a child of the calling process `myproc()` and has the PID `pid`, while ensuring safe locking. If we find such a process `p`, we undo all the locks and return `p->syscallcount`. If we loop through the entire table without finding the required process, we undo all locks and return `-1`.
   
#### Design Decisions
1.  ##### Customizable Test Cases: 
    1.  What I did: In every test file, I have included arguments `argc` and `argv`, making every testcase customizable.
    2.  Why I did it: I implemented it like this so that the user can make their own test case by inputting in the necessary arguments. This makes my testing easier and gives the user freedom in how they test the system call.
2.  ##### Zombie Processes:
    1.  What I did: Unless explicitly prohibited from being used in the problem statement, I have assumed a ZOMBIE child process to be a valid child process from which we can extract data. For example, `getchildsyscount(int pid)` works even if the process with PID `pid` is a ZOMBIE process.
    2.  Why I did it: A ZOMBIE process is a process that has exitted without having its data cleaned up. So, as long as the data is still there, I count it as a process from which we can gather its relevant information, like `getchildsyscount(int pid)`
3.  ##### `extern` for Process Table:
    1.  What I did: I declared the process table `proc` in `kernel/sysproc.c` using the `extern` keyword so that I could access the processes contained within it to make functions that required knowledge of the table and its contents possible to make.
    2.  Why I did it: This was done as it was the simplest solution I could think of to access the process table with `kernel/sysproc.c`.
#### Acknowledgements
Understood how the system calls on xv6 are implemented with the help of Stack Overflow, and used Google's Gemini as an aide in making testcases for questions C2 and C3. This README was typed out mostly by me, with the final polishing done with the help of Gemini as well. **The implementation of system calls, design decisions undertaken and the testcases for the remaining questions were all done by me.**
