//
// client.c: A very, very primitive HTTP client.
// 
// To run, try: 
//      client hostname portnumber filename
//
// Sends one HTTP request to the specified HTTP server.
// Prints out the HTTP response.
//
// For testing your server, you will want to modify this client.  
// For example:
// You may want to make this multi-threaded so that you can 
// send many requests simultaneously to the server.
//
// You may also want to be able to request different URIs; 
// you may want to get more URIs from the command line 
// or read the list from a file. 
//
// When we test your server, we will be using modifications to this client.
// 
// ***UPDATE*****
// Multithreading has been applied to this file to send multiple requests to different
// files at the same time on the command line using the format specified in the 
// README file. One can also send multiple requests from the browser at the same time.
// 

#include "io_helper.h"
#include <pthread.h>

#define MAXBUF (8192)

char *host;
int port;



// Initializations for locks and condition variables.
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;


//
// Send an HTTP request for the specified file 
//
void client_send(int fd, char *filename) {
    char buf[MAXBUF];
    char hostname[MAXBUF];
    
    gethostname_or_die(hostname, MAXBUF);
    
    /* Form and send the HTTP request */
    sprintf(buf, "GET %s HTTP/1.1\n", filename);
    sprintf(buf, "%shost: %s\n\r\n", buf, hostname);
    write_or_die(fd, buf, strlen(buf));
}

//
// Read the HTTP response and print it out
//
void client_print(int fd) {
    char buf[MAXBUF];  
    int n;
    
    // Read and display the HTTP Header 
    n = readline_or_die(fd, buf, MAXBUF);
    while (strcmp(buf, "\r\n") && (n > 0)) {
	printf("Header: %s", buf);
	n = readline_or_die(fd, buf, MAXBUF);
	
	// If you want to look for certain HTTP tags... 
	// int length = 0;
	//if (sscanf(buf, "Content-Length: %d ", &length) == 1) {
	//    printf("Length = %d\n", length);
	//}
    }
    
    // Read and display the HTTP Body 
    n = readline_or_die(fd, buf, MAXBUF);
    while (n >= 0) {
        if(n==0){
            printf("\n\n\n");
            break;
        }else{
            printf("%s", buf);
            n = readline_or_die(fd, buf, MAXBUF);
        }
	   
    }
}

//Creating a new connection
void *worker_thread_function(void *arg) {
    int clientfd;
    clientfd = open_client_fd_or_die(host, port);
    
    client_send(clientfd, arg);
    pthread_mutex_lock(&mutex);

    client_print(clientfd);
    pthread_mutex_unlock(&mutex);
    
    close_or_die(clientfd);
    pthread_exit(NULL);

}

int main(int argc, char *argv[]) {
    
    if (argc <= 3) {
    	fprintf(stderr, "Usage: %s <host> <port> <filename1> [additional-filenames]\n", argv[0]);
        fprintf(stderr,"Atleast 1 filename needed.\n");
    	exit(1);
    }
    
    host = argv[1];
    port = atoi(argv[2]);

    pthread_t threads[argc-3];

    //assigning threads with filenames from arguments
    for (int i = 0; i < argc-3; i++) {
        pthread_create(&threads[i], NULL, worker_thread_function, argv[3+i]);
    }

    pthread_exit(NULL);
}
