/* ------------------------------------------------------------------------
   phase1.c

   CSCV 452

   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <phase1.h>
#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (void *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void enableInterrupts();
static void check_deadlock();

/*ATTN: THESE NEED TO BE EDITED BEFORE SUBMITTED.*/
//extra prototypes for functionality.
void illegal_instruction_handler(int dev, void *arg);
int isZapped();
void cleanupChild(int);


/* -------------------------- Globals ------------------------------------- */

/* Patrick's debugging global variable... */
int debugflag = 1;

/* the process table */
proc_struct ProcTable[MAXPROC];

/* Process lists  */
static procPtr ReadyList;

/* current process ID */
proc_ptr Current;

/* the next pid to be assigned */
unsigned int next_pid = SENTINELPID;

/* clean up struct */
static const struct procStruct empty_struct;

int ReadyList_stack_location = 0;
static proc_ptr ReadyList[MAXPROC];

/* Number of entries in the process table */
static int num_proc_entries;
/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
	     Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup()
{
   int i;      /* loop index */
   int result; /* value returned by call to fork1() */

   /* initialize the process table */
   if (DEBUG && debugflag)
       console("startup(): initializing process table, ProcTable[]\n");
   memset(ProcTable, 0, sizeof(ProcTable));
   /* initialize current variable */
   Current = NULL;

   /* Initialize the Ready list, etc. */
   if (DEBUG && debugflag)
      console("startup(): initializing the Ready & Blocked lists\n");
   ReadyList = NULL;


   /* Initialize the clock interrupt handler */

   /* startup a sentinel process */
   if (DEBUG && debugflag)
       console("startup(): calling fork1() for sentinel\n");
   result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                   SENTINELPRIORITY);
   if (result < 0) {
      if (DEBUG && debugflag)
         console("startup(): fork1 of sentinel returned error, halting...\n");
      halt(1);
   }
  
   /* start the test process */
   if (DEBUG && debugflag)
      console("startup(): calling fork1() for start1\n");
   result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
   if (result < 0) {
      console("startup(): fork1 for start1 returned an error, halting...\n");
      halt(1);
   }

   console("startup(): Should not see this message! ");
   console("Returned from fork1 call that created start1\n");

   /*
    * possibly unneeded
    */
    ProcTable[0].next_sibling_ptr = &ProcTable[1];    // setup sentinel's sibling
    dispatcher();

    console("startup(): Should not see this message! ");
    console("Returned from fork1 call that created start1\n");
   return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish()
{
   if (DEBUG && debugflag)
      console("in finish...\n");
} /* finish */


/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
             returns -2 if stack size is less than min stack size.
             returns >= 0 for PID if process is created.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*f)(void *), void *arg, int stacksize, int priority)
{
   int proc_slot;

   if (DEBUG && debugflag)
      console("fork1(): creating process %s\n", name);

   /* test if in kernel mode; halt if in user mode */
   if(PSR_CURRENT_MODE & pst_get() == 0){
       console("fork1: is in user mode. Will halt\n");
       halt(1);
   }

   /* Return if stack size is too small */
   if(stacksize < USLOSS_MIN_STACK){
       console("fork1: stacksize < min size %d. Will halt\n", stacksize);
       return -2;
   }
   /* check for highest or lowest prio */
   if(strcmp(name, "sentienl") == 0){
       if(priority != 6){
           console("fork1: sentinel priority = %d and it must be 6\n", priority);
           return -1;
       }
   }
   else if(priority > MINPRIORITY){
       console("fork1: priority %d < minnimum %d\n", priority, MINPRIORITY);
       return -1;
   }
   else if(priority < MAXPRIORITY){
       console("fork1: priority %d > max %d \n", priority, MAXPRIORITY);
       return -1;
   }
   else if(f == NULL){
       console("fork1: function is NULL\n");
       return -1;
   }

   /* find an empty slot in the process table */
   proc_slot = proc_slot_availability();
   if(proc_slot == -1){
       console("fork1: ProcTable is full.\n");
       return -1;
   }
   proc_ptr this_process = &ProcTable[proc_slot];

   /* fill-in entry in process table */
   if ( strlen(name) >= (MAXNAME - 1) ) {
      console("fork1(): Process name is too long.  Halting...\n");
      halt(1);
   }
   strcpy(ProcTable[proc_slot].name, name);
   ProcTable[proc_slot].start_func = f;
   if ( arg == NULL )
      ProcTable[proc_slot].start_arg[0] = '\0';
   else if ( strlen(arg) >= (MAXARG - 1) ) {
      console("fork1(): argument too long.  Halting...\n");
      halt(1);
   }
   else
      strcpy(ProcTable[proc_slot].start_arg, arg);

    this_process->stacksize = stacksize;
    this_process->stack = calloc(stacksize, 1); /* create the stack and initialize its mem region to 0 */
    if(this_process->stack == NULL){ /* memory initialization issue.*/
        console("fork1: Couldn't initialize stack memory of size: %d. Will Halt\n", stacksize);
        halt(1);
    }
    this_process->priority = priority;
    this_process->pid = next_pid;
    next_pid++;

   /* Initialize context for this process, but use launch function pointer for
    * the initial value of the process's program counter (PC)
    */
   context_init(&(ProcTable[proc_slot].state), psr_get(),
                ProcTable[proc_slot].stack, 
                ProcTable[proc_slot].stacksize, launch);

   /* for future phase(s) */
   p1_fork(ProcTable[proc_slot].pid);

    /* set up the remainder of the current process */
    this_process->status = READY;
    if(Current == NULL){
        this_process->parent_proc_ptr = NULL;
    }
    else{
        this_process->parent_proc_ptr = Current;
        if(Current->child_proc_ptr == NULL){
            Current->child_proc_ptr = this_process;
        }
        else{
            this_process->next_sibling_ptr = Current->child_proc_ptr;
            Current->child_proc_ptr = this_process;
        }
    }
    if(Current){
        if(Current->child_proc_ptr){
            this_process->next_sibling_ptr = Current->child_proc_ptr;
        }
        Current->child_proc_ptr = this_process;
    }
    push_ready_list(this_process);
    //dump_slot(proc_slot);

    return this_process->pid;

} /* fork1 */
/* ATTN: LEFT OFF HERE CONTINUE TOMORROW! https://github.com/corinaserna/USLOSSProject/blob/master/phase1.c */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
   int result;

   if (DEBUG && debugflag)
      console("launch(): started\n");

   /* Enable interrupts */
   enableInterrupts();

   /* Call the function passed to fork1, and capture its return value */
   Current->status = RUNNING;
   result = Current->start_func(Current->start_arg);

   if (DEBUG && debugflag)
      console("Process %d returned to launch\n", Current->pid);

   quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
		-1 if the process was zapped in the join
		-2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *code)
{
} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int code)
{
   p1_quit(Current->pid);
} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
   proc_ptr next_process;

   p1_switch(Current->pid, next_process->pid);
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
	     processes are blocked.  The other is to detect and report
	     simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
		   and halt.
   ----------------------------------------------------------------------- */
int sentinel (void * dummy)
{
   if (DEBUG && debugflag)
      console("sentinel(): called\n");
   while (1)
   {
      check_deadlock();
      waitint();
   }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void check_deadlock()
{
} /* check_deadlock */


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
  /* turn the interrupts OFF if we are in kernel mode */
  if((PSR_CURRENT_MODE & psr_get()) == 0) {
    //not in kernel mode
    console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
    halt(1);
  } else
    /* We ARE in kernel mode */
    psr_set( psr_get() & ~PSR_CURRENT_INT );
} /* disableInterrupts */


/*
 * This section is custom functions that will help with phase1.
 * ATTN: THESE NEED TO BE EDITED BEFORE SUBMITTED.
 */
/* ATTN: CLEANED UP */
static int compare_process_priority(const void * first, const void * second)
{

    const procPtr one = *(const procPtr *)first;
    const procPtr two = *(const procPtr *)second;

    return  (one->priority - two->priority);
}
/* ATTN: CLEANED UP */
void push_ready_list(procPtr new_process)
{
    ReadyList[ReadyList_stack_location++] = new_process;

    /* sort based on priority */
    if (ReadyList_stack_location > 1 )
    {
        qsort(ReadyList, ReadyList_stack_location, sizeof (procPtr), compare_process_priority);
    }
}

procPtr popReadyList()
{
    if (ReadyListStackLoc > 0)
        ReadyListStackLoc--; // always leave sentienel on stack (ready list)

    return ReadyList[0];
}
/* ATTN: cleaned up */
int proc_slot_availability()
{
    if (num_proc_entries >= MAXPROC-1) {
        return -1;
    }
    int first_proc = 0;
    for (first_proc = 0; first_proc < MAXPROC; first_proc++)
    {
        if (ProcTable[first_proc].stackSize == 0) { // assume stacksize is 0 if unused
            ++num_proc_entries;
            return first_proc;
        }
    }
    return -1;
}
/* ATTN: CLEANED UP */
void dump_slot(int proc_slot)
{
    console("fork1(): -- (%d) Table for PID = %d\n", tableSlot, ProcTable[proc_slot].pid);
    console("%12s      [%s]\n", "name", ProcTable[proc_slot].name);
    console("%12s  [%s]\n", "startArg", ProcTable[proc_slot].startArg);
    console("%12s [%x]\n", "start_func", ProcTable[proc_slot].start_func);
    console("%12s [%d]\n", "stackSize", ProcTable[proc_slot].stackSize);
    console("%12s     [%x]\n", "stack", ProcTable[proc_slot].stack);
    console("%12s  [%d]\n", "priority", ProcTable[proc_slot].priority);
    switch(ProcTable[proc_slot].status){
        case EMPTY:
            console("%12s    [%s]\n", "status", "EMPTY");
            break;
        case READY:
            console("%12s    [%s]\n", "status", "READY");
            break;
        case RUNNING:
            console("%12s    [%s]\n", "status", "RUNNING");
            break;
        case JOIN_BLOCK:
            console("%12s    [%s]\n", "status", "JOIN_BLOCK");
            break;
        case QUIT:
            console("%12s    [%s]\n", "status", "QUIT");
            break;
        case BLOCKED:
            console("%12s    [%s]\n", "status", "BLOCKED");
            break;
    }
    console("%12s [%x]\n", "nextSiblingPtr", ProcTable[proc_slot].nextSiblingPtr);
    console("%12s    [%x]\n", "nextProcPtr", ProcTable[proc_slot].nextProcPtr);
}
void del_from_ready_list(int index)
{
    int i;
    for (i = index; i < ReadyList_stack_location-1; i++) {
        ReadyList[i] = ReadyList[i+1];
    }
    ReadyList[i] = NULL;
    ReadyList_stack_location--;
}

//
// Find this process in the readyList, remove it and mark it as blocked
//
void block_ready_proc(procPtr proc_to_block)
{
    for (int i = 0; i < ReadyList_stack_location; i++)
        if (ReadyList[i] == proc_to_block)
        {
            proc_to_block->status = BLOCKED;
            del_from_ready_list(i);
            break;
        }
}
/* ATTN: CLEANED UP */
void dump_processes(){
    for(int tableSlot = 0; tableSlot < MAXPROC; tableSlot++){
        console("fork1(): -- (%d) Table for PID = %d\n", tableSlot, ProcTable[proc_slot].pid);
        console("%12s      [%s]\n", "name", ProcTable[proc_slot].name);
        console("%12s  [%s]\n", "startArg", ProcTable[proc_slot].startArg);
        console("%12s [%x]\n", "start_func", ProcTable[proc_slot].start_func);
        console("%12s [%d]\n", "stackSize", ProcTable[proc_slot].stackSize);
        console("%12s     [%x]\n", "stack", ProcTable[proc_slot].stack);
        console("%12s  [%d]\n", "priority", ProcTable[proc_slot].priority);
        switch(ProcTable[proc_slot].status){
            case EMPTY:
                console("%12s    [%s]\n", "status", "EMPTY");
                break;
            case READY:
                console("%12s    [%s]\n", "status", "READY");
                break;
            case RUNNING:
                console("%12s    [%s]\n", "status", "RUNNING");
                break;
            case JOIN_BLOCK:
                console("%12s    [%s]\n", "status", "JOIN_BLOCK");
                break;
            case QUIT:
                console("%12s    [%s]\n", "status", "QUIT");
                break;
            case BLOCKED:
                console("%12s    [%s]\n", "status", "BLOCKED");
                break;
        }
        console("%12s [%x]\n", "nextSiblingPtr", ProcTable[proc_slot].nextSiblingPtr);
        console("%12s    [%x]\n", "nextProcPtr", ProcTable[proc_slot].nextProcPtr);
    }
}
