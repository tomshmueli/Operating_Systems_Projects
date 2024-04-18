#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <stddef.h>
#include <stdatomic.h>
#include "queue.h"

typedef struct Node {
    /* the elements of the queue of threads */
    void *data;
    struct Node *next;
} Node;

typedef struct cndNode {
    /* the elements of the queue of threads */
    cnd_t condition;
    struct cndNode *next;
} cndNode;

typedef struct waitList{
    // waiting list by FIFO order of threads
    cndNode *first;
    cndNode *last;
    int size;
} waitList;

static struct{
    // the queue of threads
    Node *first;
    Node *last;
    int size;
    mtx_t lock;
    cnd_t cond;
    size_t visited;
    waitList *wait_list;
} queue;

void initQueue(void){
    // initialize the queue
    queue.first = NULL;
    queue.last = NULL;
    queue.size = 0;
    queue.visited = 0;
    queue.wait_list = malloc(sizeof(waitList));
    queue.wait_list->first = NULL;
    queue.wait_list->last = NULL;
    queue.wait_list->size = 0;
    mtx_init(&queue.lock, mtx_plain);
}

void destroyQueue(void){
    // free the queue
    Node *node;
    cndNode *cnd_node;

    while(queue.first != NULL){
        node = queue.first;
        queue.first = queue.first->next;
        free(node);
    }

    mtx_destroy(&queue.lock);

    // destroy the waiting list
    while(queue.wait_list->first != NULL){
        cnd_node = queue.wait_list->first;
        queue.wait_list->first = queue.wait_list->first->next;
        cnd_destroy(&cnd_node->condition);
        free(cnd_node);
    }
    free(queue.wait_list);
}

void enqueue(void *data){
    // add a new node to the end of the queue
    
    //create new Node
    Node *newNode = malloc(sizeof(Node));
    newNode->data = data;
    newNode->next = NULL;


    mtx_lock(&queue.lock); // make sure no on is interrupting
    if(queue.size == 0){ // if queue is empty
        queue.first = newNode;
    } 
    else { // if queue is not empty
        queue.last->next = newNode;
    }
    queue.last = newNode;
    queue.size++;
    // signal the first thread in the waiting list
    if(queue.wait_list->size > 0){
        if(queue.wait_list->first != NULL){
            cnd_signal(&queue.wait_list->first->condition);
        } 
    }
    mtx_unlock(&queue.lock);
}

void* dequeue(void){
    /*
    remove items from the queue in FIFO order
    if the queue is empty, the calling thread should block until the queue is non-empty
    we establish a waiting list of threads that are waiting for the queue, 
    nd we take the first thread from the waiting list and wake it up when the queue is non-empty
    */ 
    void *data = NULL;
    cndNode *removed_cnd;
    cndNode *cnd_node;
    Node *removed_node;

    mtx_lock(&queue.lock); // make sure no one is interrupting

    while(queue.size == 0){ // wait until queue is not empty
        // add the thread to the waiting list as last in line
        cnd_node = malloc(sizeof(cndNode));
        cnd_init(&cnd_node->condition);
        cnd_node->next = NULL;
        if(queue.wait_list->last != NULL){
            queue.wait_list->last->next = cnd_node;
        } else {
            queue.wait_list->first = cnd_node;
        }
        queue.wait_list->last = cnd_node;
        queue.wait_list->size++;

        // wait until the queue is non-empty
        cnd_wait(&cnd_node->condition, &queue.lock);

        // remove the thread that waited first from the waiting list
        if(queue.wait_list->first != NULL){
            removed_cnd = queue.wait_list->first;
            queue.wait_list->first = queue.wait_list->first->next;
            queue.wait_list->size--;
            if(queue.wait_list->size == 0){
                queue.wait_list->last = NULL;
            }
            free(removed_cnd);
        }
    }
    // remove items from the queue in FIFO order and singal the next thread
    removed_node = queue.first;
    data = removed_node->data;
    if(queue.size == 1){
        queue.first = NULL;
        queue.last = NULL;
    }
    else{
        queue.first = queue.first->next;
    }
    queue.size--;
    free(removed_node);
    queue.visited++;
    //  signal the first thread in the waiting list
    if(queue.wait_list->size > 0 && queue.size > 0){
        if(queue.wait_list->first != NULL){
            cnd_signal(&queue.wait_list->first->condition);
        } 
    }
    
    mtx_unlock(&queue.lock);    
    return data;
}

bool tryDequeue(void **data){
    // remove items from the queue in FIFO order
    // if the queue is empty, return false
    // if the queue is non-empty, remove the first item and return true
    mtx_lock(&queue.lock); // make sure no one is interrupting
    if(queue.size == 0){
        mtx_unlock(&queue.lock);
        return false;
    }
    *data = queue.first->data;
    Node *removed_node = queue.first;
    if(queue.size == 1){
        queue.first = NULL;
        queue.last = NULL;
    } else {
        queue.first = queue.first->next;
    }
    queue.size--;
    mtx_unlock(&queue.lock);
    free(removed_node);
    queue.visited++;
    return true;
}

size_t size(void){
    // return the number of elements in the queue
    return queue.size;
}

size_t waiting(void){
    // return the number of threads waiting for the queue to be non-empty
    return queue.wait_list->size;
}

size_t visited(void){
    // return the number of elements that have been enqueued
    return queue.visited;
}
