/* Name: Sarthak Jain
*  Andrew Id: sarthak3
*/

#include "csapp.h"
#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

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
int create_requesthdrs(rio_t *rio, char *request, char *host, char *uri, int *def_port);
void parse_uri(char *uri, char *host, int *port_in_url, char *uri_without_host);
void get_host_port_header(char *value, char *host, int *port_in_header);
cnode * check_presence_cache(char *host,char *uri,int def_port);
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
	cache_initial();
	listenfd=Open_listenfd(argv[1]);
	printf("Listening port for proxy is %s and open_listenfd returned %d \n",argv[1],listenfd);
	printf("Entering while loop \n");	
	while(1)
	{
		fprintf(stderr,"While loop entered \n");
		clientlen=sizeof(struct sockaddr_storage);
		fprintf(stderr,"sizeof done \n");
		connfdp = Malloc(sizeof(int));
		fprintf(stderr,"Malloc done \n");
		*connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);
		fprintf(stderr,"Accept done \n");
		fprintf(stderr,"Calling pthread_create() in main \n");
		Pthread_create(&tid, NULL, thread, connfdp);
		fprintf(stderr,"Returned from pthread_create() in main \n");
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
	fprintf(stderr,"Calling process request in thread \n");
	process_request(connfd);
	fprintf(stderr,"Returned from process request in thread \n");
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
	char defport[MAXLINE];
	rio_t crio,srio;
	
	char *uri = Malloc(MAXLINE * sizeof(char));
    char *request = Malloc(MAXLINE * sizeof(char));
    char *host = Malloc(MAXLINE * sizeof(char));
	cnode *present_node=NULL;
	Rio_readinitb(&crio, connfd);
	
	fprintf(stderr,"Calling create requesthdrs in process request \n");
	// Create request hdrs to be sent to server
	int isGet_rqst=create_requesthdrs(&crio, request, host, uri, &def_port);
	fprintf(stderr,"Returned from create_requesthdrs in process request with isGet_rqst val %d\n",isGet_rqst);
	if(!isGet_rqst)
	{
		free(uri);
		free(request);
		free(host);
		return;
	}
	fprintf(stderr,"def port value after rqst hdrs in process request is %d \n",def_port);
	sprintf(defport,"%d",def_port);
	
	
	fprintf(stderr,"Checking for presence of request in cache \n");
	present_node=check_presence_cache(host,uri,def_port);
	if(present_node)
	{
		fprintf(stderr,"Request present in cache \n");
		delete(present_node);
		insert_front(present_node);
		if ((ssize_t) rio_writen(connfd,present_node->response,present_node->node_size) != present_node->node_size) 
		{
			unix_error("Rio_writen error");
			if(errno==EPIPE)
				fprintf(stderr,"Server closed %s \n",strerror(errno));
		}
		return;
	}
	fprintf(stderr,"Request not present in cache \n");
	
	fprintf(stderr,"Calling open_clientfd\n");
	server_fd=Open_clientfd(host,defport);
	fprintf(stderr,"returned from open_clientf with server_fd %d\n",server_fd);
	if(server_fd < 0)
	{
		fprintf(stderr,"Couldn't connect to the server with given host %s and def_port %d \n",host,def_port);
		client_error(connfd,host,"404","Page Not Found","Built Proxy couldn't connect to this server");
		free(uri);
		free(request);
		free(host);
		return;
	}
	fprintf(stderr,"server_fd is not less than 0 \n");
	Rio_readinitb(&srio, server_fd);
	fprintf(stderr,"writing the request headers to server %s\n",request);
	if ((size_t) rio_writen(server_fd, request, strlen(request)) != strlen(request)) 
	{
        unix_error("Rio_writen error");
		if(errno==EPIPE)
			fprintf(stderr,"Server closed %s \n",strerror(errno));
		Close(server_fd);
		free(uri);
		free(request);
		free(host);
		return;
    }
	fprintf(stderr,"Request written to server. \n");
	ssize_t nread;
	char temp[MAX_OBJECT_SIZE];
	fprintf(stderr,"entering loop to read response from server \n");
	while( (nread=Rio_readnb(&srio,temp,MAX_OBJECT_SIZE))!= 0 )
	{
		fprintf(stderr,"While loop entered \n");
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
				fprintf(stderr,"Server closed %s \n",strerror(errno));
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
	parse_uri(uri,host,&port_in_url,uri_without_host);
	fprintf(stderr,"Passed values are %s %s %s %d %s \n", uri, method, host, port_in_url, uri_without_host);
	sprintf(request,"%s %s %s",method,uri_without_host,default_version);
	
	strcat(request,header_user_agent);
	strcat(request,connection_hdr);
	strcat(request,proxy_connection_hdr);
	
	fprintf(stderr,"Request before strcmp in create_rqsthdrs is %s\n",request);
	fprintf(stderr,"value of buf in strcmp of create_rqst hdrs is %s \n",buf);
	while(strcmp(buf,"\r\n"))
	{
		fprintf(stderr,"While loop entered \n");	
		*key='\0';
		*value='\0';
		*key_colon='\0';
		memset(key,'\0',sizeof(key));
		memset(key_colon,'\0',sizeof(key_colon));
		memset(value,'\0',sizeof(value));
		fprintf(stderr,"Calling rio_readlineb\n");
		if( rio_readlineb(rio,buf,MAXLINE)<0 )
		{
			fprintf(stderr,"ReadLineb error in while of create_requesthdrs \n");
			return 0;
		}
		fprintf(stderr,"Returned from rio readlineb with buf value %s and length %zu\n",buf,strlen(buf));
		if(!(strcmp(buf,"\r\n")))
			break;
		
		fprintf(stderr,"Calling get other header key_colon %s\n",key_colon);
		sscanf(buf,"%s %s",key_colon,value);
		fprintf(stderr,"key_colon after scanf %s and value %s\n",key_colon,value);
		if(*key_colon!='\0' && strchr(key_colon,':')!=NULL)
			strncpy(key,key_colon,strlen(key_colon)-1);
		fprintf(stderr,"Returned from get other header with buf %s key %s value %s \n",buf,key,value);
		if(*key!='\0' && *value!='\0')
		{
			if(!strcmp(key,"Host"))
			{
				fprintf(stderr,"Calling get host port header \n");
				get_host_port_header(value,host,&port_in_url);
				fprintf(stderr,"Returned from host port header %s %d\n",host,port_in_url);
				host_passed_header=1;
			}
			if(strcmp(key,"User-Agent") && strcmp(key,"Connection") && strcmp(key,"Proxy-Connection"))
			{
				sprintf(headerline,"%s: %s\r\n",key,value);
				fprintf(stderr,"Headerline %s and len %zu\n",headerline,strlen(headerline));
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
		fprintf(stderr,"host_without_uri %s \n",host_without_uri);
		
		*slash_ptr='/';
		strcpy(uri_without_host,slash_ptr);
		fprintf(stderr,"uri_without_host %s\n",uri_without_host);
		
		port_start_ptr=strchr(host_without_uri,':');
		if(port_start_ptr!=NULL)
		{
			*slash_ptr=0;
			strcpy(port_extract, port_start_ptr + 1);
			fprintf(stderr,"Port_extract %s \n",port_extract);
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
	fprintf(stderr,"host port header() entered with value %s host %s and port %d \n",value,host,*port_in_header);
	char *colon=strchr(value,':');
	fprintf(stderr,"colon ptr at %s",colon);
	char port_val[MAXLINE];
	*port_val=0;
	if(colon!=NULL)
	{
		fprintf(stderr,"if entered \n");
		*colon=0;
		strcpy(host,value);
		fprintf(stderr,"host now is %s",host);
		*colon=':';
		strcpy(port_val,colon+1);
		fprintf(stderr,"port now is %s",port_val);
		*port_in_header=atoi(port_val);
	}
	fprintf(stderr,"returning from host port header \n");
}

cnode * check_presence_cache(char *host,char *uri,int def_port)
{
	cnode *node=NULL;
	P(&mutex);
	readcnt++;
	if(readcnt==1)
		P(&w);
	V(&mutex);
	
	node=check(host,uri,def_port);
	
	P(&mutex);
	readcnt--;
	if(readcnt==0)
		v(&w);
	v(&mutex);
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
