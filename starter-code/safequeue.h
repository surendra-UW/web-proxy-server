#define MAX_REQUESTS 256

#ifndef _SAFEQUEUE_H_
#define _SAFEQUEUE_H_

extern int max_queue_size;
struct request{
    int fd;
    int priority;
    int delay;
    char *data;
}; 

struct priority_queue{
    struct request buffer[MAX_REQUESTS];
    int size;
    pthread_mutex_t mutex;
    pthread_cond_t fill;
    pthread_cond_t empty;
};

#endif

int add_work(int fd, int priority, int delay, char *data);
struct request get_work();
void create_queue();
struct request *get_max();