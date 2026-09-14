#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>
//# define _GNU_SOURCE

# define QUANTUM_TIME 2
# define CSV_ARG_COUNT 3
# define DEFAULT_QUEUE_CAPACITY 10

typedef unsigned int u_int;

/* STRUCTS */
typedef struct{
    u_int arrival_time;
    u_int burst_time;
    u_int wait_time;
    u_int ta_time;
    u_int priority;
} Process;

typedef struct {
    u_int idx; 
    u_int size;
    u_int completed;
    Process * prcs_list;
} ProcessList;

typedef struct{
    u_int front;
    u_int back;
    u_int size;
    u_int capacity; 
    Process** processes;
} Queue;

struct prg_args {
    u_int t_flag;
    FILE * f_flag;
};

struct avg_times {
    float avg_wt;
    float avg_ta;
};

/* PROCESS METHODS */

Queue * create_queue(u_int _capacity) {
    Queue * q = (Queue*) malloc(sizeof(Queue));

    if (q == NULL) {
        return NULL;
    }

    q->front = 0;
    q->back = 0;
    q->size = 0;
    q->capacity = _capacity;

    q->processes = (Process**) malloc(sizeof(Process*) * _capacity);
    if (q->processes == NULL) {
        return NULL;
    }

    for (u_int i=0; i < q->capacity; i++) {
        q->processes[i] = NULL;
    }

    return q;

}

void create_process(Process * process, int burst_time){
    process->burst_time = burst_time;
    process->wait_time = 0;
    process->ta_time = burst_time;
}

void enqueue(Queue* queue, Process* process){
    if (process == NULL) {
        printf("attempted to enqeue a nullptr - Addition rejected\n");
        return;
    }

    if (queue->size >= queue->capacity) {
        // Create a temporary reallocation to another Process **
        u_int new_capacity = queue->capacity * 2;    
        
        Process ** temp = (Process**) realloc(queue->processes, sizeof(Process*) * new_capacity);
        if (temp == NULL) {
            printf("Reallocation of queue failed when adding process %p: %u %u %u.\n", 
                process,
                process->arrival_time, 
                process->burst_time, 
                process->priority);
            exit(EXIT_FAILURE);
        }

        queue->processes = temp;
        
        if (queue->back < queue->front) {
            u_int t = queue->front;
            int pos = new_capacity - (queue->capacity - queue->front);
            
            while (t < queue->capacity) {
                queue->processes[pos] = queue->processes[t];    // transfer the number to the new position 
                queue->processes[t] = NULL;                     // wipe the old number in temp index for clarity
                if (t == queue->front) queue->front = pos;       // change the front pointer to new position
                pos++;
                t++;
            }
        }
        
        queue->capacity = new_capacity;
    } 

    // Base Case: Inserting the first element in an empty queue. 
    // Since queue is circular, front and back could be anywhere within array
    if (queue->size == 0 && queue->front == queue->back) {
        queue->processes[queue->back] = process;
    }
    else {
        queue->processes[(++queue->back) % queue->capacity] = process;    
    } 
    queue->back = queue->back % queue->capacity;
    queue->size++;
    printf("[enqueue] Front: %u | Back: %u | Size: %u\n", queue->front, queue->back, queue->size);
}

Process* dequeue(Queue * queue) {
    if (queue->size <= 0) {
        printf("[dequeue] Queue empty. Nothing to Remove\n");
        return NULL;
    }

    Process * deq_process;
    // Base Case: One element in the queue, hence front and back indexes match
    if (queue->front == queue->back) {
        // Copy the pointer value from front of queue and replace with NULL
        deq_process = queue->processes[queue->front];
        queue->processes[queue->front] = NULL;
    }
    else {
        deq_process = queue->processes[queue->front];
        queue->processes[queue->front++] = NULL;
        queue->front = queue->front % queue->capacity;
    }

    queue->size--;
    printf("[dequeue] Removed process %p from a queue\n", deq_process);
    printf("[dequeue] Front: %u | Back: %u | Size: %u\n", queue->front, queue->back, queue->size);
    return deq_process;
}

void printQueue(Queue * q) {
    printf("[");
    for (u_int i = 0; i < (q->capacity-1); i++) {
        printf("%p, ", q->processes[i]);
    }
    printf("%p", q->processes[q->capacity-1]);
    printf("]\n");
}

/* Following mutex solution for halting infinitely-looping pthreads provided by StackOverflow User Dmitri:
 * https://stackoverflow.com/questions/7961029/how-can-i-kill-a-pthread-that-is-in-an-infinite-loop-from-outside-that-loop
 * The mutex is initially locked from main and passed into the thread args thereafter. Once the simulated CPU burst time or the quantum time 
 * limit has elapsed, main then unlocks the mutex. This is similar to a CPU's process interrupt signal. 
 * 
 * needQuit() waits for said "interrupt", securing the mutex lock once available, and subsquently returning 1 to its 
 * parent thread, ending the process' execution. 
 */
int needQuit(pthread_mutex_t* mutex) {
    switch (pthread_mutex_trylock(mutex))
    {
        case 0:
            pthread_mutex_unlock(mutex);
            return 1;

        case EBUSY: // case if lock is still held by main
            return 0;
    }
    return 1;
}

/* Simulates the execution of a process.
 * _mutex is a global variable in main used to interrupt child processes
 */
void *exec_process(void * _mutex){
    printf("[thread] Starting execution\n");
    pthread_mutex_t *mutex = (pthread_mutex_t*) _mutex;
    while(!needQuit(mutex)) {}
    printf("[thread] Interrupt acknowledged. Closing thread.\n");
    return NULL;
}

void display_error_msg()
{
    const char* invalid_argc_msg = "USER MUST INPUT TWO ARGUMENTS:\n   1. TIME QUANTUM (INTEGER) -t\n   2. LIST IN .CSV FILE WITH FORMAT -f\n\n      ARRIVAL_TIME, BURST_TIME, PRIORITY\n";
    const char* invalid_argc_msg_cont = "e.g. \'main -f list.csv -t 4\'\n";

    printf("%s\n", invalid_argc_msg);
    printf("%s\n", invalid_argc_msg_cont);
    
}

int return_args(struct prg_args * prg_args, int argc, const char * argv[]) {

    if (argc != 5) {
        display_error_msg();
        return 0;
    }
    
    for (int i = 1; i < argc; i = i+2) {
        // Check if filename is of type string, open_file() returns NULL if invalid arg
        FILE * f_flag; 
        if (0 == strcmp(argv[i], "-f") && NULL == (f_flag = fopen(argv[i+1], "r")) ) {
            printf("Invalid filename\n");
            return 0;
        } 

        if (0 == strcmp(argv[i], "-f")) {
            prg_args->f_flag = f_flag;
        }



        if (0 == strcmp(argv[i], "-t")){
            if (0 == atoi(argv[i+1]) ) {
                printf("Invalud Time Quantum. Must be integer greater than 0\n");
            return 0;
        }
            prg_args->t_flag = atoi(argv[i+1]);
        }
    }

    if (0 == prg_args->f_flag &&
        NULL == prg_args->f_flag){
            printf("Not Valid Flags\n\n");
            display_error_msg();
            return 0;
        }

    return 1;
}

void parse_csv(ProcessList * processes, FILE * file) {
    // 1. Track the line count file for dynamic array size allocation with getline()
    // 2. Rewind the file pointer to the beginning
    // 3. Split each line at the comma delimiter, assert 3 values
    // 4. Store the 3 values in a Process struct within the ProcessList
    // If error occurs, print error message and exit with EXIT_FAILURE.

    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    // 1.
    int line_count = 0;
    while (0 <= (read = getline(&line, &len, file))) line_count++;

    processes->prcs_list = (Process *) malloc(sizeof(Process) * line_count);
    printf("Number of Processes: %d\n", line_count);

    // 2.
    rewind(file);

    // 3.
    line_count = 0;
    while (-1 != (read = getline(&line, &len, file))) {
        // null terminate after the \n
        line[strcspn(line, "\n")] = 0;

        u_int * tokens = (u_int *) malloc(sizeof(u_int) * CSV_ARG_COUNT);

        char * token = strtok(line, ", ");
        int token_index = 0;

        while (token != NULL && token_index < CSV_ARG_COUNT) {

            //printf("Token not NULL:%s\n", token);
            char *endptr;
            long token_long = strtol(token, &endptr, 10);
                
                // Check for zero/negative PRIORITY values
                if ((CSV_ARG_COUNT - 1) == token_index && token_long <= 0) {
                    printf("Line %d: [%s] contains an incorrect PRIORITY value.\n", line_count+1, line);
                    printf("All PRIORITY values must be larger than zero.\n");
                    fclose(file);
                    exit(EXIT_FAILURE);
                } 

                // Add token to array and retrieve next token
                tokens[token_index++] = (u_int) token_long;
                token = strtok(NULL, ", ");
            }          
        
        // 4.
        printf("Arrival Time: %u\n", tokens[0]);
        printf("Burst Time: %u\n", tokens[1]);
        printf("Priority: %u\n\n", tokens[2]);
        processes->prcs_list[line_count].arrival_time = tokens[0];
        processes->prcs_list[line_count].burst_time = tokens[1];
        processes->prcs_list[line_count].priority = tokens[2];
        processes->prcs_list[line_count].ta_time = tokens[1];   // ta_time = burst time + wait time

        
        free(tokens);
        line_count++;
    }
    
    processes->size = line_count;

    // Close file
    fclose(file);
    free(line);
}

int compare_prcs(const void * _prcs_1, const void * _prcs_2) {
    Process * prcs_1 = (Process *) _prcs_1;
    Process * prcs_2 = (Process *) _prcs_2;
    u_int arr_time_1 = prcs_1->arrival_time;
    u_int arr_time_2 = prcs_2->arrival_time;

    if (arr_time_1 > arr_time_2) return 1;
    if (arr_time_1 < arr_time_2) return -1;
    return 0;
}

/* Program is finished only if:
 * 1. All processes in the process list have been viewed (idx == num_prcs)
 * 2. There are no processes in the execution queue
 * 3. There are no processes in the ready queue
 */
int processesRemain(ProcessList processes) {
    return (processes.completed == processes.size) ? 0 : 1;
}


/*
 * Checks if the processes remaining burst time is zero before
 * time quantum interval has elapsed
 * when prcs is NULL, return False
 */
int processFinished(Process * prcs) {
    if (prcs == NULL || prcs->burst_time != 0) return 0;
    else return 1;
}

/* 
 * Checks if a time quantum interval has elapsed
 */
int timeQuantumElapsed(u_int tq_remaining) {
    return (tq_remaining == 0) ? 1 : 0;
}

void incr_prcs_times(Queue * q) {
    for (u_int i = 0; i < q->size; i++) {
        u_int idx = (i + q->front) % q->capacity;     // make i relative to the front pointer 
        
        Process * p = q->processes[idx];
        if (p != NULL) {
            printf("[times] Process: %p | wait: %u -> %u | ta time: %u -> %u\n", p, p->wait_time, p->wait_time + 1, p->ta_time, p->ta_time+1);
            p->wait_time++;
            p->ta_time++;
        }
    }
}

/*
 * Adds any processes from the process from the current list index and past
 * that have an arrival time equal to the cpu time.
 */
void addToReadyQueue(u_int cpu_time, ProcessList * processes, Queue * ready_queue) {
    // locals to prettify
    u_int * idx = &processes->idx;
    u_int size = processes->size;
    Process * prcs_list = processes->prcs_list;
    printf("[addToReadyQueue] (First) idx: %u | num_prcs: %u | prcs_list[%u].arrival_time: %u | cpu: %u\n", 
        *idx, 
        size,
        *idx, 
        prcs_list[*idx].arrival_time, 
        cpu_time);
    // Loop until either idx matches size or next process has not yet arrived
    while (*idx < size && prcs_list[*idx].arrival_time == cpu_time) {
        Process * prcs = (*idx + prcs_list);
        printf("[addToReadyQueue] Adding Process %u, %u, %u, to ready queue at cpu time %u\n", prcs->arrival_time, prcs->burst_time, prcs->priority, cpu_time);
        enqueue(ready_queue, prcs);
        (*idx)++;
    }
}

/*
 * Transfer all processes from the ready queue to the execution queue
 */
void queueTransfer(Queue * q_src, Queue * q_dst) {
    while (q_src->size > 0) {
        Process * prcs =  dequeue(q_src);
        enqueue(q_dst, prcs);
    }
}

/* 
 * Called when the either the process' burst time or the time quantum has elapsed.
 * Current process is given permission rejoin main, and is then popped from the execution queue.
 */
void haltProcess(pthread_t thread, pthread_mutex_t * mutex) {
    printf("[haltProcess] thread ID = %lu\n", (unsigned long) thread);
    if (thread == 0) return;
    
    // Unlock the mutex and wait for the pthread to join with main thread
    pthread_mutex_unlock(mutex);
    pthread_join(thread, NULL);
}

struct avg_times calculateAvgTimes(ProcessList * processes) {
    float wt_sum = 0.0;
    float ta_sum = 0.0;

    Process *prcs = processes->prcs_list;
    for (u_int i = 0; i < processes->size; i++) {
        wt_sum += (float) (prcs+i)->wait_time;
        ta_sum += (float) (prcs+i)->ta_time;
    }

    float avg_wt = (float) wt_sum / processes->size;
    float avg_ta = (float) ta_sum / processes->size;

    struct avg_times at = {avg_wt, avg_ta};

    return at;
}

/* 
 * This program intends to simulate the round robin process scheduling algorithm. 
 *
 *  Handles the global variable pool including:
 *  - Ready and Execution Queues
 *  - Process List 
 *  - CPU current cycle (__cpu__time) 
 * 
 * 1. Init. and declare global variables through provided command line arguments
 * 2. Begin the main loop. The following steps will occur after "CPU tick" has past
 * 3. Increase __cpu__time by 1.
 * 4. Collect all processes from the list that have an ARRIVAL_TIME matching __cpu__time. Store them in the Ready Queue (RQ)
 * 5. Put the processes from the RQ into the Execution Queue (EQ). Reallocate larger memory space if required.
 * The remaining steps occur if:
 *      - Current process is completed (burst time is 0)
 *      - Time Quantum has elapsed
 * Otherwise, repeat from step 2
 * 6. If the current EQ process did not finished, enqueue back to the RQ. If so, increment the completed counter in ProcessList 
 * 7. Pop the next process from the EQ and create the thread
 * 8. Repeat the loop until all processes are completed (tracked in ProcessList struct)
 * 
 * Command Line Args:
 * [-f] .csv file
 * [-t] time quantum
 */
int main(int argc, const char * argv[]) {

    u_int __cpu__time = __UINT32_MAX__; // Max int so first iteration overflows to 0, handling processes arriving then.
    ProcessList __processes;
    __processes.idx = 0;
    __processes.size = 0;
    __processes.completed = 0;
    __processes.prcs_list = NULL;

    Queue * __ready__queue = create_queue(DEFAULT_QUEUE_CAPACITY);
    Queue * __exec__queue = create_queue(DEFAULT_QUEUE_CAPACITY);
    if (__ready__queue == NULL || __exec__queue == NULL) {
        printf("[main] Queue Memory Allocation Failure.\n");
        exit(EXIT_FAILURE);
    }

    // stores struct with default vals
    struct prg_args p_args = {0, NULL};

    if (0 == return_args(&p_args, argc, argv)){
        exit(EXIT_FAILURE);
    }

    // Store the command line arguments' values here
    const u_int t_flag = p_args.t_flag;
    FILE * f_flag = p_args.f_flag;

    // Parse the .csv file
    parse_csv(&__processes, f_flag);

    // Sort the processes based on arrival time using qsort()
    qsort(__processes.prcs_list, __processes.size, sizeof(Process), compare_prcs);

    for (u_int i = 0; i < __processes.size; i++){
        printf("%u\n", __processes.prcs_list[i].arrival_time);
    }

    // Begin main loop
    time_t ref_time = time(NULL);
    time_t curr_time;

    u_int tq_remaining = t_flag;  // Stores the remaining units of time quantum for the current process

    pthread_t thread = 0;       // No thread runs until the execution queue is non-empty. Start with no running thread
    Process* curr_prcs = NULL;  // By same logic above, start with no process referenced
    Process* prev_prcs = NULL;  // stores halted process in case of transfer back to ready queue

    pthread_mutex_t mutex;      // The mutex acts as the CPU Process interrupt. When main relinqishes its lock on the mutex, the current process will exit.
    pthread_mutex_init(&mutex, NULL);
    
    
    while (processesRemain(__processes)) {
        // Check if a second has passed since the last workflow; Only run algorithm once a second has passed
        curr_time = time(NULL);
        if (difftime(curr_time, ref_time) >= 1.0) {
            ref_time = curr_time;

            __cpu__time++;
            printf("\n[main] cpu tick: %u\n", __cpu__time);
            printf("completed: %u | size: %u\n", __processes.completed, __processes.size);

            // Increment the wait time of all processes in the EQ by one unit
            incr_prcs_times(__exec__queue);
            
            // Add all processes with an arrival time less than or equal to __cpu__time
            addToReadyQueue(__cpu__time, &__processes, __ready__queue);
            printf("[main] Ready Queue: ");
            printQueue(__ready__queue);

            if (curr_prcs != NULL) {
                printf("[main] Decrement Current process %p burst time %d -> %d\n", curr_prcs, curr_prcs->burst_time, curr_prcs->burst_time - 1);
                curr_prcs->burst_time--;
                tq_remaining--;

                // In the event of process completion or time quantum elapsing, a new process must begin execution
                if (processFinished(curr_prcs) || timeQuantumElapsed(tq_remaining)) {

                    // Check if process is complete before time quantum has elapsed
                    if (processFinished(curr_prcs)) {
                        printf("[main] Process %p completed.\n", curr_prcs);
                        haltProcess(thread, &mutex);
                        __processes.completed++;
                    }
                    // Otherwise, time quantum interval has elapsed - return unfinished process to the RQ
                    else {
                        printf("[main] Time Quantum elapsed. Halting process %p \n", curr_prcs);
                        haltProcess(thread, &mutex);
                        prev_prcs = curr_prcs;
                        if (prev_prcs != NULL) 
                            enqueue(__ready__queue, prev_prcs);
                    }

                    // Transfer all RQ processes to the EQ
                    queueTransfer(__ready__queue, __exec__queue);
                    printf("[main] Exec Queue: ");
                    printQueue(__exec__queue);

                    // Reset time quantum for next process
                    tq_remaining = t_flag;

                    // Retreive next process for execution if available
                    curr_prcs = dequeue(__exec__queue);
                    if (curr_prcs != NULL) {
                        pthread_mutex_lock(&mutex); // Lock the mutex to prevent the child thread's while-loop returning false
                        pthread_create(&thread, NULL, exec_process, &mutex);
                    } else {
                        printf("[main] No Processes available to run.\n");
                    }
                } else { // Only move new processes to exec queue
                    queueTransfer(__ready__queue, __exec__queue);
                    printf("[main] Transferring Ready Queue processes to Exec Queue: ");
                    printQueue(__exec__queue);
                }

            } else { // else branch occurs when curr_prcs is NULL. Run a process if one exists in EQ

                // Transfer all RQ processes to the EQ
                queueTransfer(__ready__queue, __exec__queue);
                printf("[main] Transferring Ready Queue processes to Exec Queue: ");
                printQueue(__exec__queue);

                // Retreive next process for execution if available
                curr_prcs = dequeue(__exec__queue);
                if (curr_prcs != NULL)
                {
                    pthread_mutex_lock(&mutex); // Lock the mutex to prevent the child thread's while-loop returning false
                    pthread_create(&thread, NULL, exec_process, &mutex);
                }
                else
                {
                    printf("[main] No Processes available to run.\n");
                }
            }
        }        
    }

    printf("\n----------------------\n");
    printf("  SIMULATION COMPLETE\n");
    printf("----------------------\n");


    // Calculate Average Wait Time and Turn-Around Time
    struct avg_times at = calculateAvgTimes(&__processes);
    printf("AVERAGE WAIT TIME:        %.3f\n", at.avg_wt);
    printf("AVERAGE TURN-AROUND TIME: %.3f\n", at.avg_ta);

    exit(EXIT_SUCCESS);
}


/*
Process related:
1. wait time variable (time spend waiting to execute)
2. turn-around time (a.k.a. TAT) variable (wait time + burst time)

Alogrithm related:
1. Change the concept of time quantum - its not a global time interval, its a fixed length of time given to each process
2. Add functions increment the wait time of each process in ready/exec queue
3. Add function to calculate the average wait time and TAT

*/