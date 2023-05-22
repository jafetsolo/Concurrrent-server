#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <sys/stat.h>
#include "request.h"
#include "io_helper.h"

#define MAXBUF (8192)

char default_root[] = ".";

// 
// Data structure to hold information about a request for the SFF algorithm.
//  
typedef struct {
    int conn_fd;
    off_t file_size;
    char *method;
    char *uri;
    char *version;
    struct stat sbuf;
} conn_info;

int buffer_count = 0;
conn_info *buffer_SFF;
int *buffer_FIFO;
int buffer_size = 1; // default buffer size is 1
int head = 0;
int tail = 0;
int num_threads = 1; // default number of threads is 1
char *schedalg = "FIFO"; // default scheduling algorithm is FIFO


// Initializations for locks and condition variables.
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;

// 
// Functions related to the FIFO scheduling algorithm. 
// The FIFO algorithm works by using queues.
// 
int is_buffer_full() {
    return (tail + 1) % buffer_size == head;
}

int is_buffer_empty() {
    return head == tail;
}

void *worker_thread_function_FIFO(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        while (is_buffer_empty()) {
            pthread_cond_wait(&empty, &mutex);
        }

        int conn_fd = buffer_FIFO[head];
        head = (head + 1) % buffer_size;
        pthread_cond_signal(&full);

        pthread_mutex_unlock(&mutex);

        request_handle_FIFO(conn_fd);
        close_or_die(conn_fd);
    }

    return NULL;
}

// 
// Functions related to the SFF scheduling algorithm.
// The SFF algorithm works by using priority queues (heaps).
// 
void swap(conn_info *a, conn_info *b) {
    conn_info temp = *a;
    *a = *b;
    *b = temp;
}

void sift_up(int index) {
    while (index > 0 && buffer_SFF[index].file_size < buffer_SFF[(index - 1) / 2].file_size) {
        swap(&buffer_SFF[index], &buffer_SFF[(index - 1) / 2]);
        index = (index - 1) / 2;
    }
}

void sift_down(int index) {
    int min_index = index;
    int left_child = 2 * index + 1;
    int right_child = 2 * index + 2;

    if (left_child < buffer_count && buffer_SFF[left_child].file_size < buffer_SFF[min_index].file_size) {
        min_index = left_child;
    }
    if (right_child < buffer_count && buffer_SFF[right_child].file_size < buffer_SFF[min_index].file_size) {
        min_index = right_child;
    }
    if (index != min_index) {
        swap(&buffer_SFF[index], &buffer_SFF[min_index]);
        sift_down(min_index);
    }
}

void insert(conn_info info) {
    buffer_SFF[buffer_count++] = info;
    sift_up(buffer_count - 1);
}

conn_info remove_min() {
    conn_info min_info = buffer_SFF[0];
    buffer_SFF[0] = buffer_SFF[--buffer_count];
    sift_down(0);
    return min_info;
}

void *worker_thread_function_SFF(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        while (buffer_count == 0) {
            pthread_cond_wait(&empty, &mutex);
        }
        conn_info info = remove_min();
        
        pthread_cond_signal(&full);

        pthread_mutex_unlock(&mutex);

        request_handle_SFF(info.conn_fd, info.method, info.uri, info.version, info.sbuf);
        close_or_die(info.conn_fd);
    }

    return NULL;
}


// main function
int main(int argc, char *argv[]) {
    int c;
    char *root_dir = default_root; // default directory is .
    int port = 10000; // default port

    if (argc != 11) {
        fprintf(stderr, "Usage: %s -d <default-root> -p <port number> -t <number of worker threads> -b <size of buffer> -s <scheduling algorithm>\n", argv[0]);
        exit(1);
    }

    while ((c = getopt(argc, argv, "d:p:t:b:s:")) != -1)
        switch (c) {
        case 'd':
            root_dir = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 't':
            num_threads = atoi(optarg);
            break;
        case 'b':
            buffer_size = atoi(optarg);
            break;
        case 's':
            schedalg = optarg;
            break;
        default:
            fprintf(stderr, "Usage: %s -d <default-root> -p <port number> -t <number of worker threads> -b <size of buffer> -s <scheduling algorithm>\n", argv[0]);
            exit(1);
        }


    // Allocating the given buffer size for the right buffer.
    if (strcasecmp(schedalg, "FIFO") == 0){
        buffer_FIFO = (int *)malloc(buffer_size * sizeof(int));    
    } else if (strcasecmp(schedalg, "SFF") == 0){
        buffer_SFF = (conn_info *)malloc(buffer_size * sizeof(conn_info));
    }

    chdir_or_die(root_dir);

    // Creating the given number of threads and assigning the right function for them.
    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        if (strcasecmp(schedalg, "FIFO") == 0){
            pthread_create(&threads[i], NULL, worker_thread_function_FIFO, NULL);
        }else if (strcasecmp(schedalg, "SFF") == 0){
            pthread_create(&threads[i], NULL, worker_thread_function_SFF, NULL);
        }
    }

    // Start listening for connections.
    int listen_fd = open_listen_fd_or_die(port);
    printf("Server Listening on Port : %i\n",port);

    while (1) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr, (socklen_t *) &client_len);
        

        // Assigning tasks to worker threads according to the given scheduling alogrithm.
        if (strcasecmp(schedalg, "FIFO") == 0){

            pthread_mutex_lock(&mutex);
            while (is_buffer_full()) {
                pthread_cond_wait(&full, &mutex);
            }

            buffer_FIFO[tail] = conn_fd;
            tail = (tail + 1) % buffer_size;
            pthread_cond_signal(&empty);

            pthread_mutex_unlock(&mutex);

        }else if (strcasecmp(schedalg, "SFF") == 0){

            struct stat sbuf;
            char buf[MAXBUF], method[MAXBUF], uri[MAXBUF], version[MAXBUF];
            char filename[MAXBUF], cgiargs[MAXBUF];

            readline_or_die(conn_fd, buf, MAXBUF);
            sscanf(buf, "%s %s %s", method, uri, version);
            request_parse_uri(uri, filename, cgiargs);
            if (stat(filename, &sbuf) == 0)
            {
                off_t file_size = sbuf.st_size;
                pthread_mutex_lock(&mutex);
                while (buffer_count == buffer_size) {
                    pthread_cond_wait(&full, &mutex);
                }

                conn_info info = {conn_fd, file_size, method, uri, version, sbuf};
                insert(info);
                pthread_cond_signal(&empty);

                pthread_mutex_unlock(&mutex);
            } else {
                request_error(conn_fd, filename, "404", "Not found", "server could not find this file");
            }
            
        }
         
        
    }

    // Freeing the used buffer after finishing listening for requests.
    if (strcasecmp(schedalg, "FIFO") == 0){
        free(buffer_FIFO);
    }
    else if (strcasecmp(schedalg, "SFF") == 0){
        free(buffer_SFF);
    }
    return 0;
}