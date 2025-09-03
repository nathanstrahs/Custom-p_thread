# Advanced Threading and Synchronization

## About The Project

This project is a custom implementation of a POSIX-style threading library (**pthread**) written in C. It provides core support for creating, managing, and synchronizing threads without relying on the system’s built-in pthread library. The goal was to explore the low-level mechanics of concurrency and synchronization, while building a practical framework that mirrors real-world threading libraries.



## Features

### Threading
- `pthread_create`, `pthread_exit`, `pthread_self`, and `pthread_join` fully implemented
- Threads are managed using a Thread Control Block (TCB) array
- Each thread is allocated its own stack and context via `setjmp/longjmp`
- Round-robin preemptive scheduling achieved through `SIGALRM`, timer interrupts every 50ms
- Cleanup of resources on `pthread_exit`

### Synchronization
- Support for semaphores, including `sem_init`, `sem_wait`, `sem_post`, `sem_destroy`
- Each semaphore maintains a thread queue for blocking and waking threads
- `lock()` and `unlock()` functions protect critical sections by blocking/unblocking signals
- Designed to prevent race conditions in concurrent execution

### Scheduler
- Preemptive, signal-driven round-robin scheduler
- Uses `SIGALRM` interrupts and `setitimer()` to trigger context switches
- Ensures fairness among threads by cycling through the ready queue
- Respects thread states (`READY`, `RUNNING`, `BLOCKED`, `EXITED`)




<!-- GETTING STARTED -->
## Highlights
- Manual stack management: Each thread is allocated a custom stack with `pthread_exit_wrapper` placed at the top
- Pointer mangling: Low-level manipulation of the jump buffer to set program counter and stack pointer registers
- Synchronization queues: Linked list–based implementation for blocked threads inside semaphores
- Context preservation: Threads save and restore CPU state using `setjmp/longjmp` mechanics
- Signal masking: Prevents scheduler interference during critical operations

## Example Usage

```
#include "threads.h"
#include <stdio.h>

sem_t sem;

void* worker(void* arg) {
    sem_wait(&sem);
    printf("Thread %d in critical section\n", pthread_self());
    sem_post(&sem);
    return NULL;
}

int main() {
    pthread_t t1, t2;
    sem_init(&sem, 0, 1);

    pthread_create(&t1, NULL, worker, NULL);
    pthread_create(&t2, NULL, worker, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    sem_destroy(&sem);
    return 0;
}
```

#### To Run:
```
$ make
$ ./main
```

<!-- CONTACT -->
## Contact

Nathan Strahs - nathanstrahs[**at**]gmail[**dot**]com

