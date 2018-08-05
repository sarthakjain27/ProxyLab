/* 
* Name: Sarthak Jain
* Andrew Id: sarthak3
*/

/*
* The following code represents a proxy server. It means
* the client's (browser) requests first go to this proxy
* server which manipulates and forms a request to be sent 
* to the actual server. On receiving response form server,
* proxy sends it back to the client. So in a way, it acts
* as an intermediary between client and the servers.
*
* The proxy also maintains a cache to store responses of some
* requests for later use. It does so that future requests doesn't
* need to be fetched from server and thus saving time. 
* LRU policy is being used and I have used a doubly linked list
* to represent cache. The head of this list stores most frequently
* used request and tail the LRU. So eviction is done from tail and
* insertion is done at start of list.
*/

#include "csapp.h"
#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <semaphore.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE (1024*1024)
#define MAX_OBJECT_SIZE (100*1024)

static const char *header_user_agent = "User Agent: Mozilla/5.0 (X11; Linux x86_64; rv:45.0) Gecko/20180601 Firefox/45.0\r\n";
static const char *default_version="HTTP/1.0\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";

/* Helper functions */
void SIGPIPE_HNDLR();
void *thread(void* vargp);
void process_request(int connfd);
int create_requesthdrs(rio_t *rio, char *request, char *host, char *uri, char *path,int *def_port);
void parse_uri(char *uri, char *host, int *port_in_url, char *uri_without_host);
void get_host_port_header(char *value, char *host, int *port_in_header);
cnode * check_presence_cache(char *host,char *uri,int def_port);
void client_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);


/* Function to handle SIGPIPE signals
* I am just printing caught statement by calling this function.
*/ 
void SIGPIPE_HNDLR()
{
    printf(" The SIGPIPE signal is caught!\n");
    return;
}

// variable to control access to critical section i.e. to cache
sem_t mutex,w; 
int main(int argc, char** argv)
{
	int listenfd,*connfdp;
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
	
	Signal(SIGPIPE,SIGPIPE_HNDLR);
	cache_initial();
	Sem_init(&mutex,0,1);
	Sem_init(&w,0,1);
	listenfd=Open_listenfd(argv[1]);
	
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
* We create a new thread to handle requests from each new client.
* We are detaching the threads so that they reap themselves.
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
* to send a request to the server. Other header in client request are appended as it is.
* 
* We have made use of cache here to first look for the built request response
* in the cache. If present we return response from cache.
* Otherwise response is fetched from server and if response size is less than 
* MAX_OBJECT_SIZE, that response is inserted in cache for later use.
* 
* The default http version is 1.0 irrespective of what version client requested for
*
*/
void process_request(int connfd)
{
	int server_fd;
	int def_port=80;
	char defport[MAXLINE];
	rio_t crio,srio;
	
	char *uri_without_path = Malloc(MAXLINE * sizeof(char));
    char *request = Malloc(MAXLINE * sizeof(char));
    char *host = Malloc(MAXLINE * sizeof(char));
	char *path=Malloc(MAXLINE * sizeof(char));
	char response[MAX_CACHE_SIZE];
	strcpy(response,"");
	int response_size=0;
	
	
	cnode *present_node=NULL;
	int found_in_cache=0; //flag for if response found in cache
	
	// Create request hdrs to be sent to server
	Rio_readinitb(&crio, connfd);
	int isGet_rqst=create_requesthdrs(&crio, request, host, uri_without_path, path ,&def_port);
	fprintf(stderr,"host %s uri %s def_port %d\n",host,uri_without_path,def_port);
	if(!isGet_rqst)
	{
		free(uri_without_path);
		free(request);
		free(host);
		free(path);
		return;
	}
	sprintf(defport,"%d",def_port);
	fprintf(stderr,"%s \n",request);
	
	P(&mutex);
	readcnt++;
	if(readcnt==1)
		P(&w);
	V(&mutex);
	present_node=check_presence_cache(uri_without_path,path,def_port);
	if(present_node)
	{
		delete_node(present_node);
		insert_front(present_node);
		if ((size_t)rio_writen(connfd,present_node->response,present_node->node_size) != present_node->node_size) 
		{
			unix_error("Rio_writen error");
			if(errno==EPIPE)
				fprintf(stderr,"Server closed %s \n",strerror(errno));
		}
		found_in_cache=1;
	}
	P(&mutex);
	readcnt--;
	if(readcnt==0)
		V(&w);
	V(&mutex);
	
	if(found_in_cache)
		return;
	
	fprintf(stderr,"Request not present in cache \n");
	
	server_fd=Open_clientfd(host,defport);
	if(server_fd < 0)
	{
		fprintf(stderr,"Couldn't connect to the server with given host %s and def_port %d \n",host,def_port);
		client_error(connfd,host,"404","Page Not Found","Built Proxy couldn't connect to this server");
		free(uri_without_path);
		free(request);
		free(host);
		free(path);
		return;
	}
	
	Rio_readinitb(&srio, server_fd);
	if ((size_t) rio_writen(server_fd, request, strlen(request)) != strlen(request)) 
	{
        unix_error("Rio_writen error");
		if(errno==EPIPE)
			fprintf(stderr,"Server closed %s \n",strerror(errno));
		Close(server_fd);
		free(uri_without_path);
		free(request);
		free(host);
		free(path);
		return;
    }
	
	ssize_t nread;
	char temp[MAX_CACHE_SIZE];
	
	while( (nread=Rio_readnb(&srio,temp,MAXLINE))!= 0 )
	{
		fprintf(stderr,"While loop entered \n");
		if( nread < 0 )
		{
			client_error(connfd,host,"404","Page Not Found","Built Proxy couldn't connect to this server");
			free(uri_without_path);
			free(request);
			free(host);
			free(path);
			return;
		}
		if ((ssize_t) rio_writen(connfd,temp,nread) != nread) 
		{
			unix_error("Rio_writen error");
			if(errno==EPIPE)
				fprintf(stderr,"Server closed %s \n",strerror(errno));
		}
		response_size+=nread;
		if(response_size<=MAX_OBJECT_SIZE)
			strcat(response,temp);
	}
	/*
	* Check if response size appropriate for insertion into cache
	*/ 
	if(response_size<=MAX_OBJECT_SIZE)
	{
		cnode *new_node=create_node(uri_without_path,path,def_port,response,response_size);
		P(&w);
		while(present_cache_size + response_size > MAX_CACHE_SIZE)
			delete_LRU();
		insert_front(new_node);
		V(&w);
	}
	Close(server_fd);
	free(uri_without_path);
	free(request);
	free(host);
	free(path);
	return;
}

/* 
* This function reads in the url entered by the user
* and splices it into key-value format separating the 
* host, uri, port.
* 
* Additionally it appends some more pre-defined headers:
* User-Agent, Connection and Proxy-Connection apart from headers
* that might already be in the input.
*/
int create_requesthdrs(rio_t *rio, char *request, char *host, char *uri, char *path ,int *def_port)
{
	int port_in_url=80;
	char buf[MAXLINE];
	char method[MAXLINE];
	char version[MAXLINE];
	char key[MAXLINE];
	char value[MAXLINE];
	char key_colon[MAXLINE];
	*key=0;
	*value=0;
	*key_colon=0;
	int host_passed_header=0;
	char headerline[MAXLINE];	
	if( rio_readlineb(rio,buf,MAXLINE)<0 )
	{
		fprintf(stderr,"ReadLineb error in create_requesthdrs \n");
		return 0;
	}
	
	sscanf(buf, "%s %s %s", method,uri,version);
	
	//Checking if the method is GET
	if(strcasecmp(method,"GET"))
		return 0;
	
	//call function to separate host, uri, port
	parse_uri(uri,host,&port_in_url,path);
	sprintf(request,"%s %s %s",method,path,default_version);
	
	strcat(request,header_user_agent);
	strcat(request,connection_hdr);
	strcat(request,proxy_connection_hdr);
	
	while(strcmp(buf,"\r\n"))
	{
		*key='\0';
		*value='\0';
		*key_colon='\0';
		memset(key,'\0',sizeof(key));
		memset(key_colon,'\0',sizeof(key_colon));
		memset(value,'\0',sizeof(value));
		if( rio_readlineb(rio,buf,MAXLINE)<0 )
		{
			fprintf(stderr,"ReadLineb error in while of create_requesthdrs \n");
			return 0;
		}
		if(!(strcmp(buf,"\r\n")))
			break;
		
		sscanf(buf,"%s %s",key_colon,value);
		if(*key_colon!='\0' && strchr(key_colon,':')!=NULL)
			strncpy(key,key_colon,strlen(key_colon)-1);
		if(*key!='\0' && *value!='\0')
		{
			if(!strcmp(key,"Host"))
			{
				get_host_port_header(value,host,&port_in_url);
				host_passed_header=1;
			}
			if(strcmp(key,"User-Agent") && strcmp(key,"Connection") && strcmp(key,"Proxy-Connection"))
			{
				sprintf(headerline,"%s: %s\r\n",key,value);
				strcat(request,headerline);
			}
		}
	}
	
	if(!host_passed_header)
	{
		sprintf(headerline,"Host: %s\r\n",host);
		strcat(request,headerline);	
	}
	*def_port=port_in_url;
	strcat(request,"\r\n");
	return 1;
}

//Helper function to splice the passed url
//to separate out host, uri and port in passed url
void parse_uri(char *uri, char *host, int *port_in_url, char *uri_without_host)
{
	char *host_start_ptr,*port_start_ptr,*slash_ptr;
	char host_without_uri[MAXLINE];
	char port_extract[MAXLINE];
	
	host_start_ptr=strstr(uri,"http://");
	if(host_start_ptr==NULL)
	{
		strcpy(uri_without_host,uri);
	}
	else
	{
		host_start_ptr=host_start_ptr+7;
		slash_ptr=strchr(host_start_ptr,'/');
		
		//make '/' after port to 0 so that strcpy thinks it is NULL character and only return val till that;
		*slash_ptr=0;
		strcpy(host_without_uri,host_start_ptr);
		
		*slash_ptr='/';
		strcpy(uri_without_host,slash_ptr);
		
		port_start_ptr=strchr(host_without_uri,':');
		if(port_start_ptr!=NULL)
		{
			*slash_ptr=0;
			strcpy(port_extract, port_start_ptr + 1);
			*port_in_url=atoi(port_extract);
		}
		strcpy(host,host_without_uri);
	}
}


/* If we have passed Host: as header filed
* then the value field contains hostname and the port
* So we search for the host and port in the header
* and update the pointers appropriately
*/ 
void get_host_port_header(char *value, char *host, int *port_in_header)
{
	char *colon=strchr(value,':');
	char port_val[MAXLINE];
	*port_val=0;
	if(colon!=NULL)
	{
		*colon=0;
		strcpy(host,value);
		*colon=':';
		strcpy(port_val,colon+1);
		*port_in_header=atoi(port_val);
	}
}

//helper function to check presence of a request in cache
cnode * check_presence_cache(char *host,char *uri,int def_port)
{
	cnode *node=NULL;
	node=check(host,uri,def_port);
	return node;
}

void client_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXLINE];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Request Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The proxy</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
