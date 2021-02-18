#define DEBUG 1

typedef struct proc_struct proc_struct;

typedef struct proc_struct * proc_ptr;

typedef struct process_queue process_queue;

struct process_queue{
    proc_ptr head;
    proc_ptr tail;
    int     size;
    int     que_type;
};

struct proc_struct {
    proc_ptr       next_proc_ptr;
    proc_ptr       child_proc_ptr;
    proc_ptr       next_sibling_ptr;
    proc_ptr       parent_proc_ptr;
    proc_ptr       next_zapped_ptr;


   char           name[MAXNAME];     /* process's name */
   char           start_arg[MAXARG]; /* args passed to process */
   context        state;             /* current context for process */
   short          pid;               /* process id */
   short          proc_table_slot;
   short          ppid;         /* parent process id */
   int            priority;
   int (* start_func) (char *);   /* function where process begins -- launch */
   char          *stack;
   unsigned int   stacksize;
   int            status;         /* READY, BLOCKED, QUIT, etc. */
   /* other fields as needed... */

   int      child_quit_status;
   short    child_quit_pid;
   int      child_has_quit;
   int      cpu_time;
   int      start_time;
   int      slice_time;

    process_queue 		child_queue;  /* queue of the process's children */
    int 			quit_status;		/* whatever the process returns when it quits */
    process_queue		dead_child_queue;	/* list of children who have quit in the order they have quit */
    proc_ptr       next_dead_sibling_ptr;
    int				zap_status; // 1 zapped; 0 not zapped
    process_queue		zap_queue;
    proc_ptr			next_zap_ptr;

    int     zapping_proc_ids[10];
    int     zap_index;

};

struct psr_bits {
        unsigned int cur_mode:1;
       unsigned int cur_int_enable:1;
        unsigned int prev_mode:1;
        unsigned int prev_int_enable:1;
    unsigned int unused:28;
};

union psr_values {
   struct psr_bits bits;
   unsigned int integer_part;
};

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY LOWEST_PRIORITY

/* defining process statuses */
#define EMPTY 0
#define READY 1
#define RUNNING 2
#define JOIN_BLOCK 3
#define ZAP_BLOCK 4
#define QUIT 5
#define BLOCKED 6

//que types
#define READYLIST 1
#define CHILDQUE  2
#define ZAPPED    3
#define DEAD      4

//quantum
#define TIMESLICE 80000