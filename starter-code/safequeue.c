#include <pthread.h>
#include "safequeue.h"

struct priority_queue *request_queue = NULL;
void create_queue() {
    struct priority_queue *queue = (struct priority_queue *)malloc(sizeof(priority_queue));
    pthread_mutex_init(&queue->mutex, 1);
    pthread_cond_init(&queue->empty, NULL);
    pthread_cond_init(&queue->full, NULL);
    queue->size = -1;
    request_queue = queue;
}

int add_work(int priority, int fd) {
    if(request_queue == NULL) return -1;

    pthread_mutex_lock(&request_queue->mutex);
    insert(priority, fd);
    return 0;

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
        swap(request_queue->buffer[parent(i)], request_queue->buffer[i]);
 
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
 
    if (l <= request_queue->size && request_queue->buffer[l] > request_queue->buffer[maxIndex]) {
        maxIndex = l;
    }
 
    // Right Child
    int r = rightChild(i);
 
    if (r <= request_queue->size && request_queue->buffer[r] > request_queue->buffer[maxIndex]) {
        maxIndex = r;
    }
 
    // If i not same as maxIndex
    if (i != maxIndex) {
        swap(request_queue->buffer[i], request_queue->buffer[maxIndex]);
        shiftDown(maxIndex);
    }
}
 
// Function to insert a new element
// in the Binary Heap
void insert(int p, int fd)
{
    request_queue->size = request_queue->size + 1;
    request_queue->buffer[request_queue->size].priority = p;
    request_queue->buffer[request_queue->size].fd = fd;
 
    // Shift Up to maintain heap property
    shiftUp(request_queue->size);
}
 
// Function to extract the element with
// maximum priority
struct request extractMax()
{
    struct request result = request_queue->buffer[0];
 
    // Replace the value at the root
    // with the last leaf
    request_queue->buffer[0] = request_queue->buffer[request_queue->size];
    request_queue->size = request_queue->size - 1;
 
    // Shift down the replaced element
    // to maintain the heap property
    shiftDown(0);
    return result;
}
 
// Function to change the priority
// of an element
// void changePriority(int i, int p)
// {
//     int oldp = request_queue->buffer[i];
//     request_queue->buffer[i] = p;
 
//     if (p > oldp) {
//         shiftUp(i);
//     }
//     else {
//         shiftDown(i);
//     }
// }
 
// Function to get value of the current
// maximum element
struct request getMax()
{
    return request_queue->buffer[0];
}
 

// void remove(int i)
// {
//     request_queue->buffer[i] = getMax() + 1;
//     shiftUp(i);
//     extractMax();
// }
 