#include <stdio.h>
#include <stdbool.h>

void *thread_pool_init(int num_threads);
bool thread_pool_hash(void *handle, const char *directory, int hash_size);
void thread_pool_shutdown(void *handle);

int main() {
    void *handle;

    handle = thread_pool_init(4); // Initialize thread pool with 4 threads
    if (handle == NULL) {
        printf("thread_pool_init: could not initialize thread pool.\n");
        return -1;
    }

    // Hash files in the specified directory with 32-bit hashing
     if (!thread_pool_hash(handle, "/home/gcadieux/CS360", 32)) {
         printf("thread_pool_hash (32 bit): failed to hash /home/gcadieux/CS360/lab7.\n");
     }

    // Hash files in the specified directory with 64-bit hashing
    if (!thread_pool_hash(handle, "/home/gcadieux/CS360", 64)) {
        printf("thread_pool_hash (64 bit): failed to hash /home/gcadieux/CS360/lab7.\n");
    }

    // Shutdown the thread pool
    thread_pool_shutdown(handle);

    return 0;
}