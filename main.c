
// #include<stdio.h>
// #include<pthread.h>
// #include<stdlib.h>

// #define THREAD_CNT 3

// // waste some time
// void *count(void *arg) {
//         unsigned long int c = (unsigned long int)arg;
//         int i;
//         for (i = 0; i < c; i++) {
//                 if ((i % 10000) == 0) {
//                         printf("tid: 0x%x Just counted to %d of %ld\n", \
//                         (unsigned int)pthread_self(), i, c);
//                 }
//         }
//     return arg;
// }

// int main(int argc, char **argv) {
//         pthread_t threads[THREAD_CNT];
//         int i;
//         unsigned long int cnt = 10000000;

//     //create THRAD_CNT threads
//         for(i = 0; i<THREAD_CNT; i++) {
//                 pthread_create(&threads[i], NULL, count, (void *)((i+1)*cnt));
//         }

//     //join all threads ... not important for proj2
//         //for(i = 0; i<THREAD_CNT; i++) {
//         //      pthread_join(threads[i], NULL);
//         //}
//     // But we have to make sure that main does not return before
//     // the threads are done... so count some more...
//     count((void *)(cnt*(THREAD_CNT + 1)));
//     return 0;
// }


#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <semaphore.h>


#define THREAD_CNT 15


sem_t newSem;

void *count(void *arg){

    printf("Thread %lu wants the lock\n", (unsigned long)pthread_self());
    sem_wait(&newSem);
    printf("Thread %lu has been given the lock\n", (unsigned long)pthread_self());
    long i;
    for(i = 0; i < 3; i++){
        printf("Thread %lu currently holds the lock\n", (unsigned long)pthread_self());
        pause();
    }

    printf("Thread %lu is done waisting time\n", (unsigned long)pthread_self());
    sem_post(&newSem);
    printf("Thread %lu no longer holds the lock\n", (unsigned long)pthread_self());
 
  return arg;
}

int main(int argc, char **argv){
	pthread_t threads[THREAD_CNT];

    sem_init(&newSem, 0, 1);

    int i;
    for(i = 0; i<THREAD_CNT; i++) 
    {
        pthread_create(&threads[i], NULL, count, &(newSem));
    }

    //for(i = 0; i<THREAD_CNT; i++) {
    //    printf("Join %d returns %d\n", i, pthread_join(threads[i], NULL));
    //}


    count((void *)(THREAD_CNT + 1));
    return 0;
}