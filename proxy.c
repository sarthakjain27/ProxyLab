/* Name: Sarthak Jain
*  Andrew Id: sarthak3
*/

#include "csapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE (1024*1024)
#define MAX_OBJECT_SIZE (100*1024)

static const char *header_user_agent = "User Agent: Mozilla/5.0"
                                    " (X11; Linux x86_64; rv:45.0)"
                                    " Gecko/20180601 Firefox/45.0";

static const char *default_version="HTTP/1.0\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";

/* Helper functions */
void SIGPIPE_HNDLR();
void *thread(void* vargp);
void process_request(int connfd);
int create_requesthdrs(rio_t *rio, char *request, char *host, char *uri, int *def_port);
void parse_uri(char *uri, char *host, int *port_in_url, char *uri_without_host);
void get_other_header(char *header, char *key, char *value);
void get_host_port_header(char *value, char *host, int *port_in_header);
void client_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);


/* Function to handle SIGPIPE signals
* I just printing handled statement by calling this function.
*/ 
void SIGPIPE_HNDLR()
{
    printf(" The SIGPIPE signal is caught!\n");
    return;
}

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
	
	listenfd=Open_listenfd(argv[1]);
	printf("Listening port for proxy is %s \n",argv[1]);
	printf("Entering while loop \n");	
	while(1)
	{
		printf("While loop entered \n");
		clientlen=sizeof(struct sockaddr_storage);
		printf("sizeof done \n");
		connfdp = Malloc(sizeof(int));
		printf("Malloc done \n");
		*connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);
		printf("Accept done \n");
		printf("Calling pthread_create() in main \n");
		Pthread_create(&tid, NULL, thread, connfdp);
		printf("Returned from pthread_create() in main \n");
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
	printf("Calling process request in thread \n");
	process_request(connfd);
	printf("Returned from process request in thread \n");
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
	int server_fd;
	int def_port=80;
	char *defport=NULL;
	rio_t crio,srio;
	
	char *uri = Malloc(MAXLINE * sizeof(char));
    char *request = Malloc(MAXLINE * sizeof(char));
    char *host = Malloc(MAXLINE * sizeof(char));
	
	Rio_readinitb(&crio, connfd);
	
	printf("Calling create requesthdrs in process request \n");
	// Create request hdrs to be sent to server
	int isGet_rqst=create_requesthdrs(&crio, request, host, uri, &def_port);
	printf("Returned from create_requesthdrs in process request with isGet_rqst val %d\n",isGet_rqst);
	if(!isGet_rqst)
	{
		free(uri);
		free(request);
		free(host);
		return;
	}
	printf("def port value after rqst hdrs in process request is %d \n",def_port);
	sprintf(defport,"%d",def_port);
	server_fd=Open_clientfd(host,defport);
	if(server_fd < 0)
	{
		printf("Couldn't connect to the server with given host %s and def_port %d \n",host,def_port);
		client_error(connfd,host,"404","Page Not Found","Built Proxy couldn't connect to this server");
		free(uri);
		free(request);
		free(host);
		return;
	}
	Rio_readinitb(&srio, server_fd);
	
	if ((size_t) rio_writen(server_fd, request, strlen(request)) != strlen(request)) 
	{
        unix_error("Rio_writen error");
		if(errno==EPIPE)
			printf("Server closed %s \n",strerror(errno));
		Close(server_fd);
		free(uri);
		free(request);
		free(host);
		return;
    }
	ssize_t nread;
	char temp[MAX_OBJECT_SIZE];
	while( (nread=Rio_readnb(&srio,temp,MAX_OBJECT_SIZE))!= 0 )
	{
		if( nread < 0 )
		{
			client_error(connfd,host,"404","Page Not Found","Built Proxy couldn't connect to this server");
			free(uri);
			free(request);
			free(host);
			return;
		}
		if ((ssize_t) rio_writen(connfd,temp,nread) != nread) 
		{
			unix_error("Rio_writen error");
			if(errno==EPIPE)
				printf("Server closed %s \n",strerror(errno));
		}
	}
	Close(server_fd);
	free(uri);
	free(request);
	free(host);
	return;
}

/* This function reads in the url entered by the user
* and splices it into key-value format separating the 
* host, uri, port.
* 
* Additionally it appends some more pre-defined headers:
* User-Agent, Connection and Proxy-Connection apart from headers
* that might already be in the input.
* 
*/
int create_requesthdrs(rio_t *rio, char *request, char *host, char *uri, int *def_port)
{
	int port_in_url=80;
	char buf[MAXLINE];
	char uri_without_host[MAXLINE];
	char method[MAXLINE];
	char version[MAXLINE];
	char *key='\0';
	char *value='\0';
	int host_passed_header=0;
	
	if( rio_readlineb(rio,buf,MAXLINE)<0 )
	{
		printf("ReadLineb error in create_requesthdrs \n");
		return 0;
	}
	
	sscanf(buf, "%s %s %s", method,uri,version);
	
	//Checking if the method is GET
	if(strcasecmp(method,"GET"))
		return 0;
	
	//call function to separate host, uri, port
	parse_uri(uri,host,&port_in_url,uri_without_host);
	printf("Passed values are %s %s %s %d %s \n", uri, method, host, port_in_url, uri_without_host);
	sprintf(request,"%s %s %s",method,uri_without_host,default_version);
	
	strcat(request,header_user_agent);
	strcat(request,connection_hdr);
	strcat(request,proxy_connection_hdr);
	
	while(strcmp(buf,"\r\n"))
	{
		*key='\0';
		*value='\0';
		if( rio_readlineb(rio,buf,MAXLINE)<0 )
		{
			printf("ReadLineb error in while of create_requesthdrs \n");
			return 0;
		}
		if(!strcmp(buf,"\r\n"))
			break;
		
		get_other_header(buf,key,value);
		if(*key!='\0' && *value!='\0')
		{
			if(!strcmp(key,"Host"))
			{
				get_host_port_header(value,host,&port_in_url);
				host_passed_header=1;
			}
			if(strcmp(key,"User-Agent") && strcmp(key,"Connection") && strcmp(key,"Proxy-Connection"))
				sprintf(request,"%s: %s\r\n",key,value);
		}
	}
	if(!host_passed_header)
	{
		sprintf(request,"Host: %s:%d\r\n",host,port_in_url);	
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
		printf("host_without_uri %s \n",host_without_uri);
		
		*slash_ptr='/';
		strcpy(uri_without_host,slash_ptr);
		printf("uri_without_host %s\n",uri_without_host);
		
		port_start_ptr=strchr(host_without_uri,':');
		if(port_start_ptr!=NULL)
		{
			*slash_ptr=0;
			strcpy(port_extract, port_start_ptr + 1);
			printf("Port_extract %s \n",port_extract);
			*port_in_url=atoi(port_extract);
		}
		strcpy(host,host_without_uri);
	}
}

//Helper functions to get name and value of passed headers
void get_other_header(char *header, char *key, char *value)
{
	char *colon,*end;
	colon=strchr(header,':');
	if(colon!=NULL)
	{
		*colon=0;
		strcpy(key,header);
		*colon=':';
		
		end=strstr(header,"\r");
		*end=0;
		strcpy(value,colon+2);
		*end='\r';
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
	char *port_val=NULL;
	if(colon!=NULL)
	{
		*colon=0;
		strcpy(host,value);
		*colon=':';
		strcpy(port_val,colon+1);
		*port_in_header=atoi(port_val);
	}
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
