#define MAX_REQUESTS 256

#ifndef _SAFEQUEUE_H_
#define _SAFEQUEUE_H_

struct request{
    int fd;
    int priority;
} request; 

struct priority_queue{
    request buffer[MAX_REQUESTS];
    int size;
    pthread_mutex_t mutex;
    pthread_cond_t full;
    pthread_cond_t empty;
} priority_queue;

#endif

int add_work(int priority, int fd);
int get_work();