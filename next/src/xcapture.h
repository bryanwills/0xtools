// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
// Copyright 2024 Tanel Poder [0x.tools]

#define TASK_COMM_LEN 16
#define MAX_STACK_LEN 127
#define MAX_FILENAME_LEN 256
#define MAX_CMDLINE_LEN 64

// kernel task states here so we don't have to include kernel headers
#define TASK_RUNNING          0x00000000
#define TASK_INTERRUPTIBLE    0x00000001
#define TASK_UNINTERRUPTIBLE  0x00000002
#define TASK_STOPPED          0x00000004
#define TASK_TRACED           0x00000008
/* Used in tsk->exit_state: */
#define EXIT_DEAD             0x00000010
#define EXIT_ZOMBIE           0x00000020
#define EXIT_TRACE (EXIT_ZOMBIE | EXIT_DEAD)
/* Used in tsk->state again: */
#define TASK_PARKED           0x00000040
#define TASK_DEAD             0x00000080
#define TASK_WAKEKILL         0x00000100
#define TASK_WAKING           0x00000200
#define TASK_NOLOAD           0x00000400
#define TASK_NEW              0x00000800
#define TASK_RTLOCK_WAIT      0x00001000
#define TASK_FREEZABLE        0x00002000
#define TASK_FREEZABLE_UNSAFE 0x00004000
#define TASK_FROZEN           0x00008000
#define TASK_STATE_MAX        0x00010000

// task flags from linux/sched.h
#define PF_KSWAPD             0x00020000  /* I am kswapd */
#define PF_KTHREAD            0x00200000  /* I am a kernel thread */

// Separate output files in CSV mode
extern FILE *sample_output_file;
extern FILE *completion_output_file;

#define SAMPLE_CSV_FILE "xcapture_samples.csv"
#define COMPLETION_CSV_FILE "xcapture_sc_completion.csv"


// global xcapture start time (for syscall duration sanitization later on)
extern __u64 program_start_time;

// to be used with BPF_MAP_TYPE_TASK_STORAGE
struct task_storage {
    __u64 sample_ktime;

    __s32 in_syscall_nr;
    __u64 sc_enter_time;
    __u64 sc_sequence_num; // any syscall entry in a task will increment this single counter

    bool  sc_sampled:1;    // task iterator will set the following fields only if it catches a task 
                           // in syscall during its sampling (later used for event completion records)
    // block I/O latency sampling (not implemented yet)
    __u64 bio_queue_ktime;
    __u32 bio_dev;
    __u64 bio_sector;
};


// to be emitted via ringbuf only on *sampled* event completions, for getting the correct measured event times
// so we are not going to emit millions of events per sec here (just up to num active threads * sampling rate)
struct sc_completion_event {
    pid_t pid;  // pid, tgid, completed_sc_sequence_nr, completed_sc_enter_time are the unique identifier here
    pid_t tgid;
    __u64 completed_sc_sequence_nr;
    __u64 completed_sc_enter_time;
    __u64 completed_sc_exit_time;
    __s32 completed_syscall_nr;
};


// use kernel nomenclature in kernel side eBPF code (pid=thread_id, tgid=process_id)
struct task_info {
    struct task_struct * addr;  // task kernel address for task storage lookups elsewhere
    pid_t pid;   // task id (tid in userspace)
    pid_t tgid;  // thread group id (pid in userspace)
    __u32 state;
    __u32 flags;

    uid_t euid;  // effective uid
    char comm[TASK_COMM_LEN];

    void * kstack_ptr;
    struct pt_regs * regs_ptr;
    __u32 thread_size;

    int kstack_len;
    __u64 kstack[MAX_STACK_LEN];
    __s32 syscall_nr;
    __u32 syscall_nr_test;
    __u64 syscall_args[6];

    char filename[MAX_FILENAME_LEN];
    char full_path[MAX_FILENAME_LEN];
    char cmdline[MAX_CMDLINE_LEN]; // userspace mem: maybe not possible to read reliably using the passive task_iter probe
    char exe_file[MAX_FILENAME_LEN];

    int debug_err;
    __u64 debug_addr;

    struct task_storage storage;
};

