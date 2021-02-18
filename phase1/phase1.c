/*
 * dispatcher must be called when a new process is executed.
 * child may have higher priority than the parent.
 */

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
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void enableInterrupts();
void disableInterrupts();
static void check_deadlock();

/*ATTN: THESE NEED TO BE EDITED BEFORE SUBMITTED.*/
//extra prototypes for functionality.
void illegal_instruction_handler(int dev, void *arg);
int isZapped();
void cleanupChild(int);
int proc_slot_availability();

void dump_processes();
void check_kernel_mode();
void time_slice(void);
void clock_handler(int device, int unit);

void add_to_queue(process_queue * que, proc_ptr process);
proc_ptr remove_from_queue(process_queue* que, proc_ptr process);
proc_ptr get_first_process_from_que(process_queue * que);
void initialize_process_queue();
void initialize_process_table(int pid);
int block(int code);
int getpid();
void update_num_processes();



/* -------------------------- Globals ------------------------------------- */

/* Patrick's debugging global variable... */
int debugflag = 0;

/* the process table */
proc_struct ProcTable[MAXPROC];

/* current process ID */
proc_ptr Current;

/* the next pid to be assigned */
unsigned int next_pid = SENTINELPID;

/* clean up struct */
static const struct proc_struct empty_struct;

/* Process lists  */
process_queue ReadyList[6];
process_queue BlockedList[6];

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
    // checks if we're in kernel mode
    check_kernel_mode();

   int i;      /* loop index */
   int result; /* value returned by call to fork1() */

   /* initialize the process table */
   if (DEBUG && debugflag)
       console("startup(): initializing process table, ProcTable[]\n");

   /* initialize current & previous variable */
   for(i = 0; i < MAXPROC; i++){
       initialize_process_table(i);
   }
    //initializes the process counter to 0;
   num_proc_entries = 0;
   //sets current process pointer to the first index of our process table
   Current = &ProcTable[0];

   /* Initialize the Ready list, etc. */
   if (DEBUG && debugflag)
      console("startup(): initializing the Ready list\n");
   //initializes readylist - queues for each priority.
    for(int i = 0; i < SENTINELPRIORITY; i++){
        initialize_process_queue(&ReadyList[i], READYLIST);
    }

   /* Initialize the clock interrupt handler */
   int_vec[CLOCK_INT] = clock_handler;

   /* startup a sentinel process */
   if (DEBUG && debugflag)
       console("startup(): calling fork1() for sentinel\n");
   //fork created for sentiel
   result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK, SENTINELPRIORITY);
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
    //debug output if we hit this our kernel is broken.
    console("startup(): Should not see this message!");
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
int fork1(char *name, int(*func)(char *), char *arg, int stacksize, int priority)
{
    // process slot. uninitialized
   int proc_slot;

   if (DEBUG && debugflag)
      console("fork1(): creating process %s\n", name);

   /* test if in kernel mode */
   //also disable interrupts when forking.
    check_kernel_mode();
    disableInterrupts();

   /* Return if stack size is too small */
   if(stacksize < USLOSS_MIN_STACK){
       console("fork1: stacksize < min size %d. Will halt\n", stacksize);
       return -2;
   }
   /* check for highest or lowest prio */
   if(strcmp(name, "sentinel") == 0){
       if(priority != 6){
           console("fork1: sentinel priority = %d and it must be 6\n", priority);
           return -1;
       }
   }
   if(priority > MINPRIORITY && (0 != strcmp(name, "sentinel"))){
       console("fork1: priority %d < minnimum %d\n", priority, MINPRIORITY);
       return -1;
   }
   if(priority < MAXPRIORITY){
       console("fork1: priority %d > max %d \n", priority, MAXPRIORITY);
       return -1;
   }
   //checks if the target function is null.
   if(func == NULL){
       console("fork1: function is NULL\n");
       return -1;
   }

   /* find an empty slot in the process table */
   proc_slot = proc_slot_availability();
    // if proc table is full
   if(proc_slot == -1){
       console("fork1: ProcTable is full.\n");
       return -1;
   }

   /* fill-in entry in process table */
   //check if name is longer than predicted.
   if ( strlen(name) >= (MAXNAME - 1) ) {
      console("fork1(): Process name is too long.  Halting...\n");
      halt(1);
   }
   strcpy(ProcTable[proc_slot].name, name);
   ProcTable[proc_slot].start_func = func;
   //check for argument
   if ( arg == NULL )
      ProcTable[proc_slot].start_arg[0] = '\0';
   else if ( strlen(arg) >= (MAXARG - 1) ) {
      console("fork1(): argument too long.  Halting...\n");
      halt(1);
   }
   else
      strcpy(ProcTable[proc_slot].start_arg, arg);
    //fill in the proctable slot
    ProcTable[proc_slot].stacksize = stacksize;
    ProcTable[proc_slot].stack = calloc(stacksize, 1); /* create the stack and initialize its mem region to 0 */
    if(ProcTable[proc_slot].stack == NULL){ /* memory initialization issue.*/
        console("fork1: Couldn't initialize stack memory of size: %d. Will Halt\n", stacksize);
        halt(1);
    }
    ProcTable[proc_slot].priority = priority;
    ProcTable[proc_slot].pid = next_pid;
    ProcTable[proc_slot].proc_table_slot = proc_slot;
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
   if(Current->pid > -1){
       add_to_queue(&Current->child_queue, &ProcTable[proc_slot]);
       if(strcmp(Current->name, "sentinel") != 0){
           ProcTable[proc_slot].parent_proc_ptr = Current;
       }
   }

    /*
     * add new process to ready list based on priority
     */
    add_to_queue(&ReadyList[priority-1], &ProcTable[proc_slot]);
    ProcTable[proc_slot].status = READY;
    //dump_slot(proc_slot);
    if(func != sentinel)
        dispatcher();
    //re-enable interrupts.
    enableInterrupts();
    //return the pid of newly created process
    return ProcTable[proc_slot].pid;

} /* fork1 */

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
      console("Process %d returned\n", Current->pid);

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
    //check if kernel mode & disable interrupts
    check_kernel_mode();
    disableInterrupts();
    //initialize child pid that will be finishing.
    int dead_child_pid = 0;
    int dead_child_proc_table_slot = 0;
    //debug
    if(DEBUG && debugflag)
        console("in join function. In join, pid = %d\n", Current->pid);
    //cannot perform a join on processes that have no children
    if(Current->child_queue.size < 1 && Current->dead_child_queue.size < 1){
        if(DEBUG && debugflag)
            console("current process has no children\n");
        return -2;
    }
    //if there is currently no children who have quit
    if(Current->dead_child_queue.size == 0){
        if(DEBUG && debugflag)
            console("join(): process %s : pid %d is being blocked\n", Current->name, Current->pid);

        block(JOIN_BLOCK);
    }
    //remove the quit child from the current processes dead child queue
    proc_ptr dead_child = remove_from_queue(&Current->dead_child_queue, Current->dead_child_queue.head);
    //obtain quit childs pid
    dead_child_pid = dead_child->pid;
    //obtain the dead childs process table slot
    dead_child_proc_table_slot = dead_child->proc_table_slot;
    //obtain the quit status.
    *code = dead_child->quit_status;
    //remove quit child from the process table & reduce number of process table entries.
    initialize_process_table(dead_child_proc_table_slot);
    num_proc_entries--;
    //if the process has been zapped. return -1.
    if(Current->zap_queue.size != 0)
        dead_child_pid = -1;

    return dead_child_pid;
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
    //check if in kernel mode & disable interrupts.
    check_kernel_mode();
    disableInterrupts();
    //debug
    if(debugflag && DEBUG)
        console("quit: called\n");
    //cannot call quit on a process that has children still.
    if(Current->child_queue.size > 0){
        console("quit(): called by process %s with pid %d with %d children\n", Current->name, Current->pid, Current->child_queue.size);
        halt(1);
    }

    //setting status codes
    Current->status = QUIT;
    Current->quit_status = code;

    //remove from ready list
    remove_from_queue(&ReadyList[(Current->priority-1)], Current);
    // no parent delete current process from process table & jump to end.
    if(Current->parent_proc_ptr == NULL){
        initialize_process_table(Current->pid);
        num_proc_entries--;
    }
    else{
        //set status in the parent process var if a parent process exists
        Current->parent_proc_ptr->child_quit_status = Current->quit_status;

        //remove from current parent process que add to parent's dead child process que.
        remove_from_queue(&Current->parent_proc_ptr->child_queue, Current);
        add_to_queue(&Current->parent_proc_ptr->dead_child_queue, Current);
        //if the parent process has been move to blocked status b/c it called join on this current process do the following
        if(Current->parent_proc_ptr->status == JOIN_BLOCK){
            //set parent to ready
            Current->parent_proc_ptr->status = READY;
            //remove parent from blocked list & add to readylist.
            add_to_queue(&ReadyList[(Current->parent_proc_ptr->priority-1)], Current->parent_proc_ptr);
            remove_from_queue(&BlockedList[(Current->parent_proc_ptr->priority-1)], Current->parent_proc_ptr);
        }
        //if current process has more than 1 process that has zapped it.
        if(Current->zap_index != 0){
            while(Current->zap_index != 0){
                //take the process that called zap.
                proc_ptr temp;
                temp = &ProcTable[Current->zapping_proc_ids[Current->zap_index-1]-1];
                //add the process to the readyList
                add_to_queue(&ReadyList[(temp->priority-1)], temp);
                //remove it from the blocked list.
                remove_from_queue(&BlockedList[(temp->priority-1)], temp);
                Current->zap_index--;
            }

        }

    }
    //if the starting process returns or calls quit, remove all processes from the process table except for start1 + sentinel
    if(strcmp(Current->name, "start1") == 0){
        for(int i = 2; i < MAXPROC; i++)
            initialize_process_table(i);
        //update the number of process entries.
        update_num_processes();
    }
    dispatcher();

} /* quit */

/*
 * This operation marks a process as being zapped.  Subsequent calls to iszapped by that process will return 1.
 * zap does not return until the zapped process has called quit.
 * The kernel  should  print  an  error  message  and  call halt(1)
 * if  a  process  tries  to  zap  itself  or attempts to zap a non- existent process.
 * Return values:
 * -1:the calling process itself was zapped while in zap
 * 0:the zapped process has called quit
 */
int zap(int pid){
    //check kernel mode & disable interrupts.
    check_kernel_mode();
    disableInterrupts();
    //create temp process ptr.
    proc_ptr temp;
    //temp points to process we want to zap
    temp = &ProcTable[pid-1];
    //add the zapping processes id to the zapped processes list. (max of 10).
    temp->zapping_proc_ids[temp->zap_index] = Current->pid;
    //increase the index counter.
    temp->zap_index++;
    //if the process tries to zap itself or the zapped process doesn't exist. halt.
    if(Current->pid == pid || temp->name[0] == '\0'){
        console("zap called by current process, or zap called by uninitialized process\n");
        halt(1);
    }
    //if zapped process has quit already. enable interrupts & return, or return -1 b/c the calling process was zapped itself.
    if(temp->status == QUIT){
        enableInterrupts();
        if(Current->zap_queue.size>0)
            return -1;
        else
            return 0;
    }
    //add parent process to the zapped processes queue.
    add_to_queue(&temp->zap_queue, Current);
    //block the zapping process.
    block(ZAP_BLOCK);
    //reenable interrupts.
    enableInterrupts();
    //if current process has been zapped. return -1.
    if(Current->zap_queue.size > 0){
        return -1;
    }
    return 0;

}
//used to get an accurate number of the processes still active in the kernel line up.
void update_num_processes(){
    int counter = 0;
    for(int i = 0; i < MAXPROC; i++){
        if(ProcTable[i].pid != -1)
            counter++;
    }
    num_proc_entries = counter;
}
// returns a positive number if the current process has been zapped.
int is_zapped(){
    check_kernel_mode();
    return(Current->zap_queue.size);
}

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
    //checks if in kernel mode and disables interrupts.
    check_kernel_mode();
    disableInterrupts();
    //creates a process pointer to the next process that needs to be executed.
    proc_ptr next_proc;

   //checks if status is Running, and checks if current processes timeslice is greater than time allotted.
    if(Current->status == RUNNING || Current->slice_time > TIMESLICE){
        //sets status of current process to ready
        Current->status = READY;
        //removes it from front of readyqueue and puts it in back
        remove_from_queue(&ReadyList[Current->priority-1], Current);
        add_to_queue(&ReadyList[Current->priority-1], Current);
    }
    //grabs highest priority process.
    for(int i = 0; i < SENTINELPRIORITY; i++){
        if(ReadyList[i].size > 0){
            next_proc = get_first_process_from_que(&ReadyList[i]);
            break;
        }
    }
    //err if process is null
    if(next_proc == NULL){
        if(DEBUG && debugflag){
            console("dispatcher(): next process is NULL\n");
            return;
        }
    }
    //sets current process to previous process
    proc_ptr previous = Current;
    //sets current process to the targeted process
    Current = next_proc;
    //sets the targeted process status to running.
    Current->status = RUNNING;


    //set time slice
    if(previous != Current){
        if(previous->pid > -1){
            previous->cpu_time += sys_clock() - previous->start_time;
        }
        Current->slice_time = 0;
        Current->start_time = sys_clock();

    }
    //for future.
    p1_switch(previous->pid, Current->pid);
    //enable interrupts.
    enableInterrupts();
    //switch to the target process.
    context_switch(&previous->state, &Current->state);
    //if we're switching to sentinel.
    if(strcmp(Current->name, "sentinel") == 0){
        //delete previous process from process table as long as it already hasn't been deleted.
        if(ProcTable[previous->pid].pid != -1){
            initialize_process_table(previous->pid);
            num_proc_entries--;
            //check for deadlock
            check_deadlock();
        }
        //or
        else{
            //check_for deadlock
            check_deadlock();
        }
    }

} /* dispatcher */
//checks if were in kernel mode.
void check_kernel_mode() {
    if (PSR_CURRENT_MODE & psr_get() == 0) {
        console("kernel function called while in user mode, by process with ID: %d, Halting...\n", Current->pid);
        halt(1);
    }
}

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
int sentinel(char * dummy)
{
    //debug
   if (DEBUG && debugflag)
      console("sentinel(): called\n");
   //forever check if were in deadlock
   while (1)
   {
      check_deadlock();
      waitint();
   }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void check_deadlock()
{
    //check if were in user mode or kernel mode & disable interrupts.
    check_kernel_mode();
    disableInterrupts();
    //if there are more than 1 process in the table & sentinel is being called. We have a problem.
    if(num_proc_entries > 1 ){
        console("not all processes have quit.\n");
        halt(1);
    }
    //else we have completed all required functions with expected output.
    else{
        console("all processes have quit. normal termination has been achieved\n");
        halt(0);
    }
} /* check_deadlock */

int block(int code){
    //check if we are in kernel mode & disables interrupts.
    check_kernel_mode();
    disableInterrupts();
    //sets the current's status to the code sent to block.
    Current->status = code;
    //removes the process from the readylist
    remove_from_queue(&ReadyList[(Current->priority-1)], Current);
    //addes the process to the blocked list.
    add_to_queue(&BlockedList[Current->priority-1], Current);
    //calls dispatcher.
    dispatcher();
    //if the current process has been zapped return -1
    if(Current->zap_queue.size > 0){
        return -1;
    }
    //return 0 if everything goes well.
    return 0;
}

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

void enableInterrupts(){
    /* turn the interrupts off if we are in kernel mode */
    if((PSR_CURRENT_MODE & psr_get()) == 0) {
        //not in kernel mode
        console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
        halt(1);
    } else
        /* We ARE in kernel mode */
        psr_set( psr_get() | PSR_CURRENT_INT );
}
//initializes the process table, additionally, this is used to delete processes from the process table.
void initialize_process_table(int pid) {

    check_kernel_mode();
    disableInterrupts();

    int i = pid % MAXPROC;
    // set up process queues.
    initialize_process_queue(&ProcTable[i].child_queue, CHILDQUE);
    initialize_process_queue(&ProcTable[i].dead_child_queue, DEAD);
    initialize_process_queue(&ProcTable[i].zap_queue, ZAPPED);
    // set status to be open
    ProcTable[i].status = EMPTY;
    // initialize pointers
    ProcTable[i].next_proc_ptr = NULL;
    ProcTable[i].next_sibling_ptr = NULL;
    ProcTable[i].next_dead_sibling_ptr = NULL;
    ProcTable[i].parent_proc_ptr = NULL;
    // initialize process vars
    ProcTable[i].pid = -1;
    ProcTable[i].start_func = NULL;
    ProcTable[i].priority = -1;
    ProcTable[i].stack = NULL;
    ProcTable[i].stacksize = 0;
    ProcTable[i].name[0] = '\0';
    ProcTable[i].start_arg[0] = '\0';
    // initialize process status / times.
    ProcTable[i].zap_status = 0;
    ProcTable[i].start_time = -1;
    ProcTable[i].cpu_time = -1;
    ProcTable[i].slice_time = 0;
    ProcTable[i].quit_status = -1;
    ProcTable[i].child_quit_status = -1;
    ProcTable[i].proc_table_slot = -1;

    for(int i = 0; i < 10; i++){ProcTable[i].zapping_proc_ids[i] = -1;}
    ProcTable[i].zap_index = 0;

    num_proc_entries--;
    enableInterrupts();
}
//initializes queues needed to keep track of
void initialize_process_queue(process_queue * queue, int type){
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->que_type = type;
}

//add a process to end of the specified que.
void add_to_queue(process_queue *que, proc_ptr process){
    if(que->head == NULL && que->tail ==NULL){
        que->head = que->tail = process;
    }
    else{
        if (que->que_type == READYLIST)
            que->tail->next_proc_ptr = process;
        else if(que->que_type == CHILDQUE)
            que->tail->next_sibling_ptr = process;
        else if (que->que_type == ZAPPED)
            que->tail->next_zapped_ptr = process;
        else if (que->que_type == DEAD)
            que->tail->next_dead_sibling_ptr = process;
        que->tail = process;
    }

    que->size++;

}
//remove a process from the specified que, where ever it may lie.
proc_ptr remove_from_queue(process_queue * que, proc_ptr process) {
    proc_ptr temp = que->head;
    proc_ptr temp2;
    if (que->head == NULL) {
        return NULL;
    }
    if (que->head == que->tail) {
        que->head = que->tail = NULL;
    }
    else {
        if (que->que_type == READYLIST){
            temp2 = temp->next_proc_ptr;
            //if process to be removed is first in que
            //remove process and set head = to second in line.
            if(temp == process){
                que->head = temp2;
                que->size--;
                return temp;
            }
            while(temp2 != process){
                temp = temp->next_proc_ptr;
                temp2 = temp2->next_proc_ptr;
            }
            if(temp2 == process) {
                if (temp2->next_proc_ptr != NULL) {
                    temp->next_proc_ptr = temp2->next_proc_ptr;
                    que->size--;
                    return temp2;
                }
            }
            temp->next_proc_ptr = NULL;
            que->size--;
            return temp2;
        }
        else if (que->que_type == CHILDQUE){
            temp2 = temp->next_sibling_ptr;
            //if process to be removed is first in que
            //remove process and set head = to second in line.
            if(temp == process){
                que->head = temp2;
                que->size--;
                return temp;
            }
            while(temp2 != process){
                temp = temp->next_sibling_ptr;
                temp2 = temp2->next_sibling_ptr;
            }
            if(temp2 == process){
                if(temp2->next_sibling_ptr != NULL) {
                    temp->next_sibling_ptr = temp2->next_sibling_ptr;
                    que->size--;
                    return temp2;
                }
            }
            temp->next_sibling_ptr = NULL;
            que->size--;
            return temp2;
        }
        else if (que->que_type == ZAPPED){
            temp2 = temp->next_zapped_ptr;
            //if process to be removed is first in que
            //remove process and set head = to second in line.
            if(temp == process){
                que->head = temp2;
                que->size--;
                return temp;
            }
            while(temp2 != process){
                temp = temp->next_zapped_ptr;
                temp2 = temp2->next_zap_ptr;
            }
            if(temp2 == process) {
                if (temp2->next_zapped_ptr != NULL) {
                    temp->next_zapped_ptr = temp2->next_zapped_ptr;
                    que->size--;
                    return temp2;
                }
            }
            temp->next_zap_ptr = NULL;
            que->size--;
            return temp2;
        }
        else if (que->que_type == DEAD) {
            temp2 = temp->next_dead_sibling_ptr;
            //if process to be removed is first in que
            //remove process and set head = to second in line.
            if(temp == process){
                que->head = temp2;
                que->size--;
                return temp;
            }
            while(temp2 != process){
                temp = temp->next_dead_sibling_ptr;
                temp2 = temp2->next_dead_sibling_ptr;
            }
            if(temp2 == process) {
                if (temp2->next_dead_sibling_ptr != NULL) {
                    temp->next_dead_sibling_ptr = temp2->next_dead_sibling_ptr;
                    que->size--;
                    return temp2;
                }
            }
            temp->next_dead_sibling_ptr = NULL;
            que->size--;
            return temp2;
        }
    }
    que->size--;
    return temp;
}
//return the first process from the the specified que w.o. removing
proc_ptr get_first_process_from_que(process_queue * que){
    if(que->head == NULL)
        return NULL;
    else
        return que->head;
}
//returns pid of current process
int getpid() {
    check_kernel_mode();
    return Current->pid;
}
//finds the first process slot available in our process table.
int proc_slot_availability()
{
    //if process table is full return -1
    if (num_proc_entries >= MAXPROC) {
        return -1;
    }
    int first_proc = 0;
    for (first_proc; first_proc < MAXPROC; first_proc++)
    {
        //if process table slot stacksize is 0 then the process table slot is unused, return this slot.
        if (ProcTable[first_proc].stacksize == 0) {
            ++num_proc_entries;
            return first_proc;
        }
    }
    return -1;
}
//dumps all the process data from the process table in a format that is easily readable. Will also print the statuses in a readable format.
void dump_processes() {
    console("%12s %12s %12s %12s %12s %12s %12s\n", "PID", "Parent", "Priority", "Status", "# Kids", "CPUTime", "Name");
    for (int tableSlot = 0; tableSlot < MAXPROC; tableSlot++) {
        if (tableSlot == 0 || tableSlot == 1) {
            console("%12d %12d %12d ", ProcTable[tableSlot].pid, -2, ProcTable[tableSlot].priority);
            switch (ProcTable[tableSlot].status) {
                case EMPTY:
                    console("%12s ", "EMPTY");
                    break;
                case READY:
                    console("%12s ", "READY");
                    break;
                case RUNNING:
                    console("%12s ", "RUNNING");
                    break;
                case JOIN_BLOCK:
                    console("%12s ", "JOIN_BLOCK");
                    break;
                case ZAP_BLOCK:
                    console("%12s ", "ZAP_BLOCK");
                    break;
                case QUIT:
                    console("%12s ", "QUIT");
                    break;
                case BLOCKED:
                    console("%12s ", "BLOCKED");
                    break;
                default:
                    console("%12d ", ProcTable[tableSlot].status);
            }
            console("%12d %12d %12s\n", ProcTable[tableSlot].child_queue.size, ProcTable[tableSlot].cpu_time,
                    ProcTable[tableSlot].name);
        } else {
            if(ProcTable[tableSlot].status == EMPTY){
                console("%12d %12d %12d ", ProcTable[tableSlot].pid, -1, ProcTable[tableSlot].priority);
                switch (ProcTable[tableSlot].status) {
                    case EMPTY:
                        console("%12s ", "EMPTY");
                        break;
                    case READY:
                        console("%12s ", "READY");
                        break;
                    case RUNNING:
                        console("%12s ", "RUNNING");
                        break;
                    case JOIN_BLOCK:
                        console("%12s ", "JOIN_BLOCK");
                        break;
                    case ZAP_BLOCK:
                        console("%12s ", "ZAP_BLOCK");
                        break;
                    case QUIT:
                        console("%12s ", "QUIT");
                        break;
                    case BLOCKED:
                        console("%12s ", "BLOCKED");
                        break;
                    default:
                        console("%12d ", ProcTable[tableSlot].status);

                }
                console("%12d %12d %12s\n", ProcTable[tableSlot].child_queue.size, ProcTable[tableSlot].cpu_time, "\0");
                continue;
            }
            console("%12d %12d %12d ", ProcTable[tableSlot].pid, ProcTable[tableSlot].parent_proc_ptr->pid, ProcTable[tableSlot].priority);
            switch (ProcTable[tableSlot].status) {
                case EMPTY:
                    console("%12s ", "EMPTY");
                    break;
                case READY:
                    console("%12s ", "READY");
                    break;
                case RUNNING:
                    console("%12s ", "RUNNING");
                    break;
                case JOIN_BLOCK:
                    console("%12s ", "JOIN_BLOCK");
                    break;
                case ZAP_BLOCK:
                    console("%12s ", "ZAP_BLOCK");
                    break;
                case QUIT:
                    console("%12s ", "QUIT");
                    break;
                case BLOCKED:
                    console("%12s ", "BLOCKED");
                    break;
                default:
                    console("%12d ", ProcTable[tableSlot].status);

            }
            console("%12d %12d %12s\n", ProcTable[tableSlot].child_queue.size, ProcTable[tableSlot].cpu_time, ProcTable[tableSlot].name);
        }
    }
}
/*
 * this operation calls the dispatcher if the currently executing process has exceeded its allotted run time.
 */
void time_slice(void){

    check_kernel_mode();
    disableInterrupts();

    Current->slice_time = sys_clock() - Current->start_time;
    if(Current->slice_time > TIMESLICE){
        Current->slice_time = 0;
        dispatcher();
    }
    else
        enableInterrupts();
}

/*
 * returns the time in ms at which the currently executing process began its current time slice
 */
int read_cur_start_time(void){
    check_kernel_mode();
    return Current->start_time/1000;
}
//this function calls the time_slice function & keeps track of the number of interrupts.
void clock_handler(int device, int unit){
    static int count = 0;
    count++;
    if(DEBUG && debugflag)
        console("clockhandler called %d times\n", count);
    time_slice();
}
//blocks the current process and sets the status to the status specified.
int block_me(int new_status){

    check_kernel_mode();
    disableInterrupts();

    if (new_status < 10) {
        console("new_status < 10 \n");
        halt(1);
    }

    return block(new_status);
}
//unblocks the target process identified by the pid variable
int unblock_proc(int pid){
    //checks if in kernel mode & disables interrupts.
    check_kernel_mode();
    disableInterrupts();
    //makes sure the PID is within the processtables range.
    int i = 0;
    i = pid % MAXPROC;
    //checks if the specified process table entry is equal to our target process, or if the status has not been blocked, or if the current process is the one being unblocked.
    if (ProcTable[i-1].pid != pid || ProcTable[i-1].status <= 10 || Current->pid == pid)
        return -2;
    //creates a new process pointer temp
    proc_ptr temp;
    //temp points to target process
    temp = &ProcTable[i-1];
    //temp status is now ready.
    temp->status = READY;
    //remove temp from the blocked list que and add it to the back of the ready list que
    add_to_queue(&ReadyList[temp->priority-1], temp);
    remove_from_queue(&BlockedList[temp->priority-1], temp);
    //calls dispatcher
    dispatcher();
    //if target process has been zapped.
    if (Current->zap_queue.size > 1)
        return -1;
    return 0;
}