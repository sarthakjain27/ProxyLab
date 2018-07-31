/* Name: Sarthak Jain
*  Andrew Id: sarthak3
*/

#include "csapp.h"
#include <stdio.h>
#include <stdlib.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE (1024*1024)
#define MAX_OBJECT_SIZE (100*1024)

static const char *header_user_agent = "Mozilla/5.0"
                                    " (X11; Linux x86_64; rv:45.0)"
                                    " Gecko/20180601 Firefox/45.0";

/* Helper functions */
void SIGPIPE_HNDLR(int sig);
void *thread(void* vargp);
void process_request(int connfd);
int generate_request(rio_t *rio, char *request, char *host, char *uri, int *port);

/* Function to handle SIGPIPE signals
* I just printing handled statement by calling this function.
*/ 
void SIGPIPE_HNDLR(int sig)
{
    printf(" The SIGPIPE signal is caught!\n");
    return;
}

int main(int argc, char** argv)
{
	int listenfd,port,*connfdp;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	pthread_t tid;
	
	/* Check number of command line arguments 
	* 
	* And if more than 2 (executable file name and port) arguments
	* have been provided. 
	*/ 
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
	
	port=atoi(argv[1]);
	
	Signal(SIGPIPE,SIGPIPE_HNDLR);
	
	listenfd=Open_listenfd(port);
	
	while(1)
	{
		clientlen=sizeof(struct sockaddr_storage);
		connfdp = Malloc(sizeof(int));
		*connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);
		Pthread_create(&tid, NULL, thread, connfdp);
	}
	
    return 0;
}

/* Thread routine 
* We create a new thread to handle each new client
*/
void *thread(void* vargp)
{
	int connfd = *((int *)vargp);
	
	/* Detaching the new thread so it can be
	* reaped automatically on its finish.
	*/ 
	Pthread_detach(Pthread_self());
	free(vargp);
	process_request(connfd);
	Close(connfd);
	return NULL;
}

/* This is the main routine which performs the fetch from server
* and serves the client. 
*
* It has helper functions to first splice the given HTTP request in (method uri version) format
* along with Host header fields.
* It looks for a given port in the request otherwise uses default 80 port (given for HTTP)
* It also appends 3 more headers: User-Agent, Connection and Proxy-Connection
* to send a request to the server. 
* 
* The default http version is 1.0 irrespective of what version client requested for
*
* 
*/
void process_request(int connfd)
{
	int serverfd;
	int def_port=80;
	rio_t crio,srio;
	
	char *uri = Malloc(MAXLINE * sizeof(char));
    char *request = Malloc(MAXLINE * sizeof(char));
    char *host = Malloc(MAXLINE * sizeof(char));
	
	Rio_readinitb(&client_rio, fd);
	
	// Create request hdrs to be sent to server
	int isGet_rqst=create_requesthdrs(&crio, request, host, uri, &def_port);
}

/*
*
*/
int generate_request(rio_t *rio, char *request, char *host, char *uri, int *port)
{
	
}

