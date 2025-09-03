#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdint.h>
#include <sys/time.h>
#include <semaphore.h>
#include "threads.h"

#define READY 0
#define RUNNING 1
#define EXITED 2
#define BLOCKED 3

//only want to call the all threads creator 1 time
bool firstCall = true;

//keeps track of the current number of threads and their indecies in the TCB table
int threadIndex = 0;
int runningThreads = 0;
int currentThread = 0;

int pthread_create(pthread_t *thread,
    const pthread_attr_t *attr,
    void *(*start_routine) (void *),
    void *arg
);
void pthread_exit(void *value_ptr);
pthread_t pthread_self(void);
void allocateMain();
void setupScheduler();
void lock();
void unlock();
int pthread_join(pthread_t thread, void **value_ptr);
int sem_init(sem_t *sem, int pshared, unsigned value);
int sem_wait(sem_t *sem);
int sem_post(sem_t *sem);
int sem_destroy(sem_t *sem);
void pthread_exit_wrapper();


struct TCB
{
    int status;
    pthread_t ID;
    unsigned long* spointer;
    jmp_buf context;
    void* exitCode;
    int waiting;
};
//global variable to keep track of all the current threads
struct TCB allThreads[128];

struct Node{
    int thread;
    struct Node* next;
};
struct threadQueue{
    struct Node* head;
    struct Node* tail;
};

void addToQueue(struct threadQueue* q, int mythread){
    struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
    newNode->thread = mythread;
    newNode->next = NULL;

    if(q->head==NULL){
        q->head = newNode;
        q->tail = newNode;
        return;
    }
    q->tail->next = newNode;
    q->tail = q->tail->next;
    return;
}
int removeFromQueue(struct threadQueue* q){
    if(q->head == NULL){
        perror("ERROR:");
        exit(0);
    }
    struct Node* oldHead = q->head;
    int returnThread = oldHead->thread;
    q->head = q->head->next;
    if(q->head == NULL){
        q->tail = NULL;
    }
    free(oldHead);
    return returnThread;
}

void freeThreadQueue(struct threadQueue* q){
    while(q->head != NULL){
        removeFromQueue(q);
    }
    free(q);
}

bool isEmpty(struct threadQueue* q){
    if(q->head == NULL){
        return true;
    }
    return false;
}

struct semaphore
{
    int value;
    bool init;
    struct threadQueue* q;
};


void scheduler(int num){
    if(allThreads[currentThread].status != EXITED && allThreads[currentThread].status != BLOCKED){
        allThreads[currentThread].status = READY;
    }

    // Save the context 
    if (setjmp(allThreads[currentThread].context) == 0) {
        // Round-robin find the next thread that is ready to run
        do {
            currentThread = (currentThread + 1) % threadIndex;
        } while (allThreads[currentThread].status != READY);
        allThreads[currentThread].status = RUNNING;
        
        // Restore the context of the next thread using longjmp
        longjmp(allThreads[currentThread].context, 1);
    }
}

// only want to call this one time
void setupScheduler(){
    struct sigaction sig;
    sig.sa_handler = scheduler;
    sig.sa_flags = SA_NODEFER;
    sigemptyset(&sig.sa_mask);

    sigaction(SIGALRM, &sig, NULL);

    // set up the timer for every 50 ms
    struct itimerval timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 50000; 
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 50000;

    setitimer(ITIMER_REAL, &timer, NULL);
}


pthread_t pthread_self(void){
    return currentThread;
}

void pthread_exit(void *value_ptr){
    // get id of the calling thread and assign exit code
    int id = pthread_self();
    allThreads[id].exitCode = value_ptr;

    //if there are any threads waiting on id to finish then unblock it
    if(allThreads[id].waiting != -1){
        allThreads[allThreads[id].waiting].status = READY;
    }
    allThreads[id].status = EXITED;
    
    runningThreads--;

    if(runningThreads >0){
        scheduler(0);
    }
    exit(0);
}

// schedule the main
void allocateMain(){
    // threadIndex should be 0
    allThreads[0].spointer = NULL;
    allThreads[0].status = RUNNING;
    allThreads[0].ID = 0;
    allThreads[0].waiting = -1;
    currentThread = 0;
    runningThreads = 1;
    setjmp(allThreads[0].context);
    threadIndex=1;
}


int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg){

    if(firstCall == true){
        allocateMain();
        setupScheduler();
        firstCall = false;
    }
    
    struct TCB* newTCB = &allThreads[threadIndex];
    
    newTCB->spointer=malloc(32767);
    unsigned long* stkPointer = (unsigned long *) (((char*)allThreads[threadIndex].spointer)+32767);
    // moves to the top of the stack
    stkPointer--;
    // puts pthread_exit at the top of the stack
    *(stkPointer) = (unsigned long)pthread_exit_wrapper;
    newTCB->ID = threadIndex;
    newTCB->status = READY;
    newTCB->exitCode = NULL;
    newTCB->waiting = -1;

    runningThreads++;

    // deals with mangling the pointers and storing them in the jump buff
    setjmp(newTCB->context);

    // stack pointer
    newTCB->context[0].__jmpbuf[6] = ptr_mangle((unsigned long int)stkPointer);

    // program counter
    newTCB->context[0].__jmpbuf[7] = ptr_mangle((unsigned long int)start_thunk);

    // R12 points to start routine
    newTCB->context[0].__jmpbuf[2] = (unsigned long int)start_routine;

    // R13 points to arg
    newTCB->context[0].__jmpbuf[3] = (unsigned long int)arg;
    *thread = newTCB->ID;
    threadIndex++;

    return 0;
}

void pthread_exit_wrapper(){
    unsigned long int res;
    asm("movq %%rax, %0\n":"=r"(res));
    pthread_exit((void *) res);
}

void lock(){
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGALRM);
    sigprocmask(SIG_BLOCK, &signals, NULL);
}

void unlock(){
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &signals, NULL);
}

int pthread_join(pthread_t thread, void **value_ptr){
    // typecast the pthread to an int
    int threadLocation = thread;
    int id = pthread_self();

    //if waiting ona thread that already exited
    if(allThreads[threadLocation].status == EXITED){
        if(value_ptr!=NULL){
            *value_ptr = allThreads[threadLocation].exitCode;
        }
        if(allThreads[threadLocation].spointer){
            free(allThreads[threadLocation].spointer);
            allThreads[threadLocation].spointer=NULL;
        }
    }
    else{
        allThreads[id].status = BLOCKED;
        allThreads[threadLocation].waiting = id;
        scheduler(0);
        if(value_ptr!=NULL){
            *value_ptr = allThreads[threadLocation].exitCode;
        }
        if(allThreads[threadLocation].spointer){
            free(allThreads[threadLocation].spointer);
            allThreads[threadLocation].spointer=NULL;
        }
    }
    /*
    check whether thread has exited
    if yes?
        assign value_ptr to exit code of thread
        free the resources
    if no?
        calling thread needs to block
    */
   return 0;
}

int sem_init(sem_t *sem, int pshared, unsigned value){
    struct semaphore* mySem = (struct semaphore*)malloc(sizeof(struct semaphore));
    mySem->init = true;
    mySem->q = (struct threadQueue*)malloc(sizeof(struct threadQueue));
    mySem->q->head = NULL;
    mySem->q->tail = NULL;
    mySem->value = value;

    sem->__align = (intptr_t) mySem;
    return 0;
}


int sem_wait(sem_t *sem){
    struct semaphore* mySem = (struct semaphore*)sem->__align;

    if(mySem->value > 0){
        mySem->value--;
        return 0;
    }
    else{
        int threadToAdd = pthread_self();
        addToQueue(mySem->q, threadToAdd);
        allThreads[threadToAdd].status = BLOCKED;
        scheduler(0);
        // add current thread to the queue
        // block the thread
        // schedule
    }
    return 0;

}
int sem_post(sem_t *sem){
    struct semaphore* mySem = (struct semaphore*)sem->__align;

    if(!isEmpty(mySem->q)){
        int thread = removeFromQueue(mySem->q);
        allThreads[thread].status = READY;
    }else{
        (mySem->value)++;
    }
    return 0;

    /*
    if there is a thread in the queue
    set the threads status to ready

    if there are no threads in teh queue then increment the semaphore value
    */
}
int sem_destroy(sem_t *sem){
    struct semaphore* mySem = (struct semaphore*)sem->__align;
    freeThreadQueue(mySem->q);

    mySem->init = false;
    free(mySem);
    return 0;
}


