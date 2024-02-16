// tpool.c
// Gabriel Cadieux
// Last Edited: 11/7/23
// Calculates hash values for all files in a directory using pthreads

// Libraries
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <pthread.h>

// Structs
typedef enum {
    WORK,
    RESULT,
    DIE
} WorkCommand;

// Worker Struct
typedef struct {
    pthread_t tid;
    WorkCommand command;
    uint64_t hash_value;
    int hash_type;
    FILE *working_file;
    char *file_name;
    pthread_mutex_t lock;
} Thread;

// Threadpool struct
typedef struct {
    Thread *workers;
    int num_threads;
} ThreadPool; 

// Hash32 and hash64 calculate the 32bit or 64bit hash value of a given FILE
uint32_t hash32(FILE *fl) 
{
    uint32_t digest = 2166136261;
    char byte;
    while ((byte = fgetc(fl)) != EOF) {
        digest ^= byte;
        digest *= 16777619;
    }
    return digest;
}

uint64_t hash64(FILE *fl) 
{
    uint64_t digest = 14695981039346656037u;
    int byte;
    while ((byte = fgetc(fl)) != EOF) {
        digest ^= byte;
        digest *= 1099511628211u;
    }
    return digest;
}

// Worker thread function
static void *worker(void *arg) 
{
    // Our thread will block until it receives the DIE or WORK command
    Thread *t = (Thread * )arg;
    for (;;) 
    {
        // Blocks here
        pthread_mutex_lock(&t->lock);
        if (t->command == DIE) 
        {
            break;
        }
        // When asked to work, the thread will calculate either hash32 or hash64 of the working file
        // based on the given hash_size in thread_pool_hash, then mark command as RESULT
        else if (t->command == WORK) 
        {
            if (t->hash_type == 32) 
            {
                t->hash_value = hash32(t->working_file);
                t->command = RESULT;
            }
            if (t->hash_type == 64) 
            {
                t->hash_value = hash64(t->working_file);
                t->command = RESULT;
            }
        }
    }

    return NULL;
}

// Initializes thread pool given number of threads to make
void *thread_pool_init(int num_threads) 
{
    // Return NULL if given num_threads is not in range [1, 32]
    if ((num_threads < 1) || (num_threads > 32)) 
    {
        return NULL;
    }
    // Allocate the threadpool and then allocate the workers
    ThreadPool *threadpool; 
    threadpool = calloc(num_threads, sizeof(ThreadPool));
    threadpool->workers = (Thread *)calloc(num_threads, sizeof(Thread));
    threadpool->num_threads = num_threads;

    // Initilize the worker's mutexes and create the threads
    for (int i = 0; i < num_threads; i++) 
    {
        pthread_mutex_init(&threadpool->workers[i].lock, NULL);
        pthread_mutex_lock(&threadpool->workers[i].lock);
        pthread_create(&threadpool->workers[i].tid, NULL, worker, threadpool->workers + i);
    }

    return threadpool;
}

// Hashes all files in a given directory either by 32hash or 64hash based on hash_size
bool thread_pool_hash(void *handle, const char *directory, int hash_size) 
{
    // If hash_size, handle, or directory messed up return false
    if ((hash_size != 32) && (hash_size != 64)) 
    {
        return false;
    }
    if (handle == NULL) 
    {
        return false;
    }
    
    DIR *direct;
    direct = opendir(directory);
    // If opendir failed, return false
    if (!direct) 
    {
        return false;
    }
    // Setup some variables and directory variables to utilize while loop
    ThreadPool *TP = (ThreadPool *)handle;
    struct dirent *dent;
    int num_files = 0;
    int queued = 0;

    // While we are still queueing up or getting results, loop
    while (1) 
    {
        // This 2 for loop setup is much like pthreadprimescontrolled
        // We will attempt to queue up work to the number of threads we have
        for (int i = 0; i < TP->num_threads; i++) 
        {
            dent = readdir(direct);
            // If entry is NULL, do nothing
            if (dent != NULL) 
            {
                // If entry is a regular file, give the file to the current worker and other relevent information
                if (dent->d_type == DT_REG)
                {
                    queued++;
                    // Do some strcpy and strcat shenanigans to get the correct file path
                    char *s = (char *)malloc(strlen(directory) + 1 + strlen(dent->d_name) + 1);
                    strcpy(s, directory);
                    strcat(s, "/");
                    strcat(s, dent->d_name);
                    TP->workers[i].working_file = fopen(s, "r");
                    free(s);

                    if (TP->workers[i].working_file == NULL) {
                        perror("Error opening file\n");
                    }
                    TP->workers[i].hash_type = hash_size;
                    TP->workers[i].file_name = dent->d_name;
                    TP->workers[i].command = WORK;
                    pthread_mutex_unlock(&TP->workers[i].lock);
                }
                // else, it is not a regular file, decrement i since we are not assigning work
                else 
                {
                    // I realize a while loop with a counting variable would have been better, but this works
                    i--;
                }
            }
        }
        // wait on results for the number of jobs queued and print them when we get them
        for (int i = 0; i < queued; i++) 
        {
            while(TP->workers[i].command != RESULT) 
            {
                nanosleep(&(struct timespec){0, 250000}, NULL);
            }
            if (hash_size == 32) 
            {
                printf("%08x: %s/%s\n", TP->workers[i].hash_value, directory, TP->workers[i].file_name);
                fclose(TP->workers[i].working_file);
            }
            if (hash_size == 64) 
            {
                printf("%016llx: %s/%s\n", TP->workers[i].hash_value, directory, TP->workers[i].file_name);
                fclose(TP->workers[i].working_file);
            }
        }
        // If we were not able to queue enough jobs to give to all our threads, then there is no more work, break while loop
        if (queued != TP->num_threads) 
        {
            break;
        }
        queued = 0;
    }
    // Close the directory
    closedir(direct);
    return true;
}

// Shutdown the threadpool, iterate through the workers to send DIE command, 
// join them to the parent and destroy their mutexes
void thread_pool_shutdown(void *handle) 
{
    if (handle == NULL) 
    {
        return;
    }
    ThreadPool *TP = (ThreadPool *)handle;
    for (int i = 0; i < TP->num_threads; i++) 
    {
        TP->workers[i].command = DIE;
        pthread_mutex_unlock(&TP->workers[i].lock);
        pthread_join(TP->workers[i].tid, NULL);
        pthread_mutex_destroy(&TP->workers[i].lock);
    }
    // Also free threadpool's workers memory and free the threadpool itself
    free(TP->workers);
    free(TP);
    return;
}