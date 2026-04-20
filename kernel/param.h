#define NPROC        64  // maximum number of processes
#define NCPU          8  // maximum number of CPUs
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NFRAME       64  // frames per system
#define SWAPSIZE   1175  // frames per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGBLOCKS    (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define SWAPBLOCKS   (SWAPSIZE * 4)  // max swap blocks
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       2000 + SWAPBLOCKS  // size of file system in blocks
#define SWAPSTART    FSSIZE - SWAPBLOCKS
#define MAXPATH      128   // maximum file path name
#define USERSTACK    1     // user stack pages
//MLFQ MACROs
// #define Q0           2     // Time Quantum in Queue 0
// #define Q1           4     // Time Quantum in Queue 1
// #define Q2           8     // Time Quantum in Queue 2
// #define Q3           16     // Time Quantum in Queue 3
// #define QUANTUM(i)   ((i)==0?Q0:((i)==1?Q1:((i)==2?Q2:Q3))) //Time quantum per queue level
#define QUANTUM(i)   (2 << (i)) //Time quantum per queue level

