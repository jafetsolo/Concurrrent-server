# Overview

This codebase is an extension of [https://github.com/remzi-arpacidusseau/ostep-projects/tree/master/concurrency-webserver](https://github.com/remzi-arpacidusseau/ostep-projects/tree/master/concurrency-webserver) providing 2 additional features to the webserver.

- Multithreading - to allow the server to serve multiple requests concurrently.
- Scheduling Algorithm - using two algorithms (explained below) to schedule

## Part 1: Multithreading

This project utilizes the pthread library to implement multithreading in C. The pthread_create() function is used to create threads, allowing the program to execute multiple tasks concurrently. To prevent data races and ensure thread safety, the pthread_mutex_init(), pthread_mutex_lock(), and pthread_mutex_unlock() functions are used to initialize, lock, and unlock mutexes, respectively. Additionally, the pthread_cond_init(), pthread_cond_wait(), and pthread_cond_signal() functions are used to create, wait for, and signal condition variables, which allows threads to synchronize their execution and communicate with each other.

## Part 2: Scheduling Algorithms

### First In First Out

First In First Out (FIFO) is a simple scheduling algorithm where the first request that comes in is the first one to be served. In the context of a web server, requests are received by the server and are placed in a queue. The worker threads then pick up requests from the queue in the order they were received and process them. To implement FIFO scheduling with worker threads, a shared queue is used to store incoming requests. When a worker thread becomes available, it dequeues the next request in the queue and processes it.

### Shortest File First

Smallest File First (SFF) is a scheduling algorithm that prioritizes smaller requests over larger ones. In the context of a web server, the size of a request is determined by the size of the file being requested. When a request is received, its size is checked and compared to the sizes of other requests in the priority queue. The request with the smallest file size is then given priority and processed first. To implement SFF scheduling with worker threads, a shared priority queue is used to store incoming requests along with their file sizes and other information about them such as the method, uri, and HTTP version. When a worker thread becomes available, it dequeues the smallest request in the queue based on the file size and processes it.

# Code Organization

The code organization is roughly the same as the one in original project. There are some HTML files that could be requests by clients and some of them are in folders. There has been a change in the `Makefile` to accomodate for the threading changes. The major changes happen in the `wclient`, `request` and `wserver` files. Firstly three more command line arguments have been added: number of threads, the size of the buffer, and the type of scheduling algorithm. Secondly, in the `request` file the `request_handle` has been split into `request_handle_FIFO` AND `request_handle_SFF` functions to handle the two algorithms. The implementation of the `request_handle_FIFO` function is almost the same as the previous implementation of the `request_handle` function but the `request_handle_SFF` function has more parameters to accept as the Shortest File First algorithm requires that a preprocessing is done on the request to figure out the requested file size. The `404 - file not found` error has also been moved to the `wserver` file. In the `wserver` file, two functions have been implemented to pass to the worker threads when they are created, one for each scheduling algorithm. Different buffers (queue and priority queue) with their own necessary functions have been implemented. A struct for the connection descriptor of the Shortest File First algorithm has also been created.Finally inorder to test the algorithms the `wclient` needed to be modified , it's multithreaded and instead of accepting only one filename in the command line it accepts atleast one (or multiple filenames) according to the command line format below.

# Running the code

To compile the code:

```sh
make or make "all"
```

To run the server:

```sh
./wserver -d <default-dir> -p <port> -t <number of threads> -b <size of buffer> -s <scheduling algorithm>
```

To run the client:

```sh
./wclient <host> <port> <filename1> [additional-filenames]
```

# Challenges

One of the challenging part of the project was figuring out how to organize the code for the Shortest File First algorithm. In the First In First Out algorithm, it's easy to figure out how to populate the buffer with connection description but in the Shortest File First algorithm to put requests in the priority queue buffer, first the file needs to be checked if it exists, second the file size needs to be calculated, and thrid the request (with all the information associated with it) needs to be pushed to the priority queue (heap). This meant changing the request.c file too and figuring out a way to organize both these files took some time.
Another thing that required more thought was implementing the heap functions: inserting, removing, sifting up, sifting down, and swapping. That took some time to implement.
