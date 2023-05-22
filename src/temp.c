#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <sys/stat.h>
#include "request.h"
#include "io_helper.h"

char default_root[] = ".";
typedef struct {
    int conn_fd;
    off_t file_size;
} conn_info;
conn_info *buffer;
int buffer_size;
int buffer_count = 0;
int num_threads;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;

void swap(conn_info *a, conn_info *b) {
    conn_info temp = *a;
    *a = *b;
    *b = temp;
}

void sift_up(int index) {
    while (index > 0 && buffer[index].file_size < buffer[(index - 1) / 2].file_size) {
        swap(&buffer[index], &buffer[(index - 1) / 2]);
        index = (index - 1) / 2;
    }
}

void sift_down(int index) {
    int min_index = index;
    int left_child = 2 * index + 1;
    int right_child = 2 * index + 2;

    if (left_child < buffer_count && buffer[left_child].file_size < buffer[min_index].file_size) {
        min_index = left_child;
    }
    if (right_child < buffer_count && buffer[right_child].file_size < buffer[min_index].file_size) {
        min_index = right_child;
    }
    if (index != min_index) {
        swap(&buffer[index], &buffer[min_index]);
        sift_down(min_index);
    }
}

void insert(conn_info info) {
    buffer[buffer_count++] = info;
    sift_up(buffer_count - 1);
}

conn_info remove_min() {
    conn_info min_info = buffer[0];
    buffer[0] = buffer[--buffer_count];
    sift_down(0);
    return min_info;
}

void *worker_thread(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        while (buffer_count == 0) {
            pthread_cond_wait(&empty, &mutex);
        }

        conn_info info = remove_min();
        pthread_cond_signal(&full);

        pthread_mutex_unlock(&mutex);

        request_handle(info.conn_fd);
        close_or_die(info.conn_fd);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    int c;
    char *root_dir = default_root;
    int port = 10000;

    while ((c = getopt(argc, argv, "d:p:t:b:")) != -1)
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
        default:
            fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t num_threads] [-b buffer_size]\n");
            exit(1);
        }

    buffer = (conn_info *)malloc(buffer_size * sizeof(conn_info));

    chdir_or_die(root_dir);

    int listen_fd = open_listen_fd_or_die(port);

    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, worker_thread, NULL);
    }

    while (1) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        int conn_fd = accept_or_die(listen_fd, (sockaddr_t *)&client_addr, (socklen_t *)&client_len);

        char filename_buf[1024];
        int is_static;
        struct stat sbuf;
        if (request_parse_uri(conn_fd, filename_buf, &is_static) == 0 && stat(filename_buf, &sbuf) == 0) {
            off_t file_size = sbuf.st_size;

            pthread_mutex_lock(&mutex);
            while (buffer_count == buffer_size) {
                pthread_cond_wait(&full, &mutex);
            }

            conn_info info = {conn_fd, file_size};
            insert(info);
            pthread_cond_signal(&empty);

            pthread_mutex_unlock(&mutex);
        } else {
            close_or_die(conn_fd);
        }
    }

    free(buffer);
    return 0;
}