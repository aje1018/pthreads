#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>

#include "concurrentBuffer.h"

#define MAX_T 12

char** copyargv;

pthread_t consumers[MAX_T];
pthread_t producers[MAX_T];

pthread_mutex_t mu;

long finalHist[45];

void* buffer;

// allow this to be overridden at compile time
// e.g. gcc -DBUFFER_SIZE=1 ...
#ifndef BUFFER_SIZE
#define BUFFER_SIZE 10
#endif

// the work functions for the threads
static void *work1(void *);
static void *work2(void *);



int main(int argc, char *argv[])
{
    long i;
    int numThreads;
    if (argc < 2)
    {
        fprintf(stderr, "Usage: ./histogram <file1.txt> <file2.txt> ...\n");
        exit(-1);
    }

    numThreads = argc-1;
    copyargv = malloc((numThreads) * sizeof(char*));

    for (i = 0; i < numThreads; i++) {
        copyargv[i] = malloc((strlen(argv[i + 1]) + 1) * sizeof(char));
        strcpy(copyargv[i], argv[i + 1]);
        //printf("%s\n", copyargv[i]);
    }

    // create buffer 
    buffer = createConcurrentBuffer(BUFFER_SIZE);
    if (buffer == NULL)
    {
        fprintf(stderr, "can't create the concurrent buffer\n");
        exit(-1);
    }

    for (i = 0; i < numThreads; i++) {

        // create producer thread
        if (pthread_create(&producers[i], NULL, work1, (void*)i) != 0) {
            fprintf(stderr, "error in thread create for producers");
        }

        // create consumer thread
        if (pthread_create(&consumers[i], NULL, work2, (void*)i) != 0) {
            fprintf(stderr, "error in thread create for consumers");
        }

    }

    for (i = 0; i < numThreads; i++) {
        // wait for producers
        if (pthread_join(producers[i], NULL)) {
            fprintf(stderr, "join for producers fails\n");
            exit(-1);
        }

        //void* ret; // this should be histogram for each text doc
        // wait for consumers
        if (pthread_join(consumers[i], NULL)) {
            fprintf(stderr, "join for consumers fails\n");
        }

        // sum up histograms into global histogram
    }

    // free concurrent buffer
    deleteConcurrentBuffer(buffer);

    for (int i = 0; i < 45; i++) {
        printf("%d %ld\n", i + 6, finalHist[i]);
    }
    //printf("%ld\n", sum);
}
#define MAX_LINE_LENGTH 1024
static char* getLine(FILE* fp)
{
    char buf[MAX_LINE_LENGTH];
    int i = 0;
    //if (fp == NULL) return NULL;
    int c = getc(fp);
    if (c == EOF) return NULL;
    while (c != EOF && c != '\n')
    {
        buf[i] = c;
        i += 1;
        if (i == MAX_LINE_LENGTH)
        {
            fprintf(stderr, "maximum line length (%d) exceeded\n", MAX_LINE_LENGTH);
            exit(-1);
        }
        c = getc(fp);
    }
    if (c == '\n')
    {
        buf[i] = c;
        i += 1;
    }
    buf[i] = 0;
    char* s = malloc(i + 1);
    if (s == NULL)
    {
        fprintf(stderr, "malloc failed in getLine\n");
        exit(-1);
    }
    strcpy(s, buf);
    return s;
}


// work function for the first thread (the producer)
#define MAX_BLOCK_LENGTH 1000
static void *work1(void *in)
{
    FILE* fp = fopen(copyargv[(long)in], "r");

    // read the lines in the file
    char *line;
    char* block = malloc(2 * MAX_BLOCK_LENGTH); // extra 100 spots in case it runs over 1000 char
    while ((line = getLine(fp)) != NULL)
    {
        
        if ((strlen(block) + strlen(line)) >= MAX_BLOCK_LENGTH) {
            strcat(block, line);
            putConcurrentBuffer(buffer, block);
            //printf("%s\n", block);
            //free(block);
            
            block = NULL;
            block = malloc(2 * MAX_BLOCK_LENGTH);
            block[0] = '\0';
        }
        else {
            strcat(block, line);
        }
        free(line);
        
    }
    if (strlen(block) != 0) {
        putConcurrentBuffer(buffer, block);
        //free(block);
    }
    // put a NULL to indicate EOF
    putConcurrentBuffer(buffer, NULL);
    return NULL;
}

// work function for the second thread (the consumer)
static void *work2(void *in)
{
    //long* hist = malloc(44 * sizeof(long));
    char *line;
    //
    while ((line = getConcurrentBuffer(buffer)) != NULL)
    {

        int j = 0;
		// process block of lines -> words of lengths 6-50
        pthread_mutex_lock(&mu);
        for (int i = 0; i < strlen(line); i++) {
            while (isalpha(line[i]) && i < strlen(line)) {
                j++;
                i++;
            }
            if (j >= 6 && j <= 50) {
                //printf("here: %s\n", hold);
                finalHist[j - 6]++;
            }
            while (!isalpha(line[i]) && i < strlen(line)) {
                i++;
            }
            i--;
            j = 0;
        }
        pthread_mutex_unlock(&mu);
        // add up all words w/ lengths
        free(line);
    }
    free(line);
    //
    return NULL;
}

