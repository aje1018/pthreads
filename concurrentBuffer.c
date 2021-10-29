//
// defines a concurrent buffer that supports the producer-consumer pattern
//
// the buffer elements are simply void* pointers
// note: the buffer does not make a copy of the data.
//       it simply stores a pointer to the data.
// 
// the buffer is FIFO (first in, first out)
//

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "concurrentBuffer.h"


typedef struct control {
    struct buffer* buff; // head of linked list -> first in queue
    unsigned int buffSize;
    unsigned int numInBuff;
} control;

typedef struct buffer {
    void* data;
    struct buffer* next;
} buffer;


pthread_mutex_t mu;

pthread_cond_t get;
pthread_cond_t put;


// create a concurrent buffer
//   size: the number of elements in the buffer
//   returns: a handle to be used to operate on the buffer
void *createConcurrentBuffer(unsigned int size)
{
    // initialize mutex
    if (pthread_mutex_init(&mu, NULL) != 0) fprintf(stderr, "can't init mutex");

    // initialize condition variable
    if (pthread_cond_init(&get, NULL) != 0) fprintf(stderr, "can't init condition variable");
    if (pthread_cond_init(&put, NULL) != 0) fprintf(stderr, "can't init condition variable");

    control* table = malloc(sizeof(control));
    if (table == NULL) {
        return NULL;
    }
    table->buffSize = size;
    table->numInBuff = 0;
    return (void*)table;
}

// put a value in a buffer
//   handle: handle for a concurrent buffer
//   p: pointer to be put in the buffer
//   note: calling thread will block until there is space in the buffer
void putConcurrentBuffer(void *handle, void *p)
{
    control* table = (control*)handle;

    // create new pointer in buffer
    buffer* newBuff = malloc(sizeof(buffer));
    newBuff->data = p;
    newBuff->next = NULL;

    // lock mutex
    if (pthread_mutex_lock(&mu) != 0) fprintf(stderr, "error in mutex_lock in producer");
        
    while (table->buffSize == table->numInBuff) { // wait until space is available
        pthread_cond_wait(&put, &mu);
    }
    if (table->numInBuff == 0) {
        table->buff = newBuff;
    }
    else {
        buffer* traverse = table->buff;
        while (traverse->next != NULL) {
            traverse = traverse->next;
        }
        traverse->next = newBuff;
    }
    table->numInBuff++;
            
    // signal consumer condition variable that data is available
    if (pthread_cond_signal(&get) != 0) fprintf(stderr, "error in cond_signal in producer");

    // unlock mutex
    if (pthread_mutex_unlock(&mu) != 0) fprintf(stderr, "error in mutex_unlock in producer");
}

// get a value from a buffer
//   handle: handle for a concurrent buffer
//   returns: pointer retrieved from buffer
//   note: calling thread will block until there is a value available
void *getConcurrentBuffer(void *handle)
{
    control* table = (control*)handle;
    
    // lock mutex
    if (pthread_mutex_lock(&mu) != 0) fprintf(stderr, "error in mutex_lock in consumer");
        
    while (table->numInBuff == 0) { // wait until data is available
        pthread_cond_wait(&get, &mu);
    }

    buffer* temp = table->buff;
    table->buff = table->buff->next;
    table->numInBuff--;

    void* ret = temp->data;
    free(temp);

    // signal producer condition variable that data has been removed
    if (pthread_cond_signal(&put) != 0) fprintf(stderr, "error in cond_signal in consumer");

    // unlock mutex 
    if (pthread_mutex_unlock(&mu) != 0) fprintf(stderr, "error in mutex_unlock in consumer");
    return ret;
}

// delete a buffer
//   handle: handle for the concurrent buffer to be deleted
void deleteConcurrentBuffer(void *handle)
{
    control* table = (control*)handle;
    buffer* buff = table->buff;
    while (buff) {
        buffer* del = buff;
        buff = buff->next;
        free(del->data);
        free(del);
    }
    free(buff);
    free(table);
}

