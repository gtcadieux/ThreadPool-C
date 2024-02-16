This program utilizes pthreads in C to create a pool of threads that then hash all files in a given directory.
An example MAIN.c program is provided that can be altered to test on your own machine.
The key parts to change are the number in thread_pool_init(X) and the directories that the if statements are looking in.
