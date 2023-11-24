#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include "safequeue.h"

int insert(int fd, int p, int delay);

struct priority_queue *request_queue = NULL;
void create_queue() {
    printf("creating queue \n");
    struct priority_queue *queue = (struct priority_queue *)malloc(sizeof(struct priority_queue));
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->empty, NULL);
    pthread_cond_init(&queue->fill, NULL);
    queue->size = -1;
    request_queue = queue;
}

int add_work(int fd, int priority, int delay) {
    if(request_queue == NULL) return -1;
    printf("inserting queue priority %d delay %d fd %d\n", priority, delay, fd);
    return insert(fd, priority, delay);
}
 
int parent(int i)
{
    return (i - 1) / 2;
}
 

int leftChild(int i)
{
    return ((2 * i) + 1);
}
 
int rightChild(int i)
{
    return ((2 * i) + 2);
}
 
void swap(struct request *a, struct request* b) {
    struct request temp = *a;
    *b = *a;
    *a = temp;
}
// Function to shift up the node in order
// to maintain the heap property
void shiftUp(int i)
{
    while (i > 0 && request_queue->buffer[parent(i)].priority < request_queue->buffer[i].priority) {
 
        // Swap parent and current node
        swap(&request_queue->buffer[parent(i)], &request_queue->buffer[i]);
 
        // Update i to parent of i
        i = parent(i);
    }
}
 
// Function to shift down the node in
// order to maintain the heap property
void shiftDown(int i)
{
    int maxIndex = i;
 
    // Left Child
    int l = leftChild(i);
 
    if (l <= request_queue->size && request_queue->buffer[l].priority > request_queue->buffer[maxIndex].priority) {
        maxIndex = l;
    }
 
    // Right Child
    int r = rightChild(i);
 
    if (r <= request_queue->size && request_queue->buffer[r].priority > request_queue->buffer[maxIndex].priority) {
        maxIndex = r;
    }
 
    // If i not same as maxIndex
    if (i != maxIndex) {
        swap(&request_queue->buffer[i], &request_queue->buffer[maxIndex]);
        shiftDown(maxIndex);
    }
}
 
// Function to insert a new element
// in the Binary Heap
int insert(int fd, int p, int delay)
{
    pthread_mutex_lock(&request_queue->mutex);
    while (request_queue->size+1 == max_queue_size) {
        // pthread_mutex_unlock(&request_queue->mutex);
        // return -1;
        pthread_cond_wait(&request_queue->empty, &request_queue->mutex);
    }
    request_queue->size = request_queue->size + 1;
    request_queue->buffer[request_queue->size].fd = fd;
    request_queue->buffer[request_queue->size].priority = p;
    request_queue->buffer[request_queue->size].delay = delay;
    printf("inserted into queue fd is %d\n", request_queue->buffer[request_queue->size].fd);
    // Shift Up to maintain heap property
    shiftUp(request_queue->size);
    pthread_cond_signal(&request_queue->fill);
    pthread_mutex_unlock(&request_queue->mutex);
    return 0;
}
 
// Function to extract the element with
// maximum priority
struct request get_work()
{
    printf("extracting queue element %d\n", request_queue->size);
    pthread_mutex_lock(&request_queue->mutex);
    while(request_queue->size == -1)
        pthread_cond_wait(&request_queue->fill, &request_queue->mutex);
    struct request result = request_queue->buffer[0];
    printf("extracted queue element %d\n", result.fd);
    // Replace the value at the root
    // with the last leaf
    request_queue->buffer[0] = request_queue->buffer[request_queue->size];
    request_queue->size = request_queue->size - 1;
 
    // Shift down the replaced element
    // to maintain the heap property
    shiftDown(0);
    pthread_cond_signal(&request_queue->empty);
    pthread_mutex_unlock(&request_queue->mutex);
    return result;
}
 
// maximum element
struct request getMax()
{
    return request_queue->buffer[0];
}
 
