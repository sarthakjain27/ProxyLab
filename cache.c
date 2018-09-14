/*
 * This file contains the functions
 * used for implementing cache in my web 
 * proxy server.
 *
 * Consider the cache as a doubly linked list. Where
 * insertions happen at head of list and removal
 * happend from rear of list. 
 * 
 * Meaning Most recently used is at head of tail
 * and most recently used is at rear of tail.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cache.h"
#include <strings.h>
#include <string.h>
#include <semaphore.h>


/*
* Initialize the cache with default values
*
*/ 
void cache_initial()
{
	head=NULL;
	tail=NULL;
	rqst_in_cache=0;
	present_cache_size=0;
	readcnt=0;
}

/*
* Function to check for the presence of a request in cache
*/
cnode * check(char *host,char *uri,int def_port)
{
	cnode *i=head;
	while(i)
	{
		if((!strcasecmp(i->hostname,host)) && (!strcasecmp(i->pathname,uri)) && i->port==def_port)
			return i;
		i=i->next;
	}
	return NULL;
}

//function to create a new node and store the passed values
cnode * create_node(char *hostname, char *pathname, int port, char *response, size_t response_size)
{
	cnode *node=(cnode *)malloc(sizeof(cnode));
	node->hostname=malloc(strlen(hostname)+1);
	strcpy(node->hostname,hostname);
	node->pathname=malloc(strlen(pathname)+1);
	strcpy(node->pathname,pathname);
	node->port=port;
	node->response=malloc(strlen(response)+1);
	strcpy(node->response,response);
	node->node_size=response_size;
	node->next=NULL;
	node->prev=NULL;
	return node;
}

//Function to delete a node from cache
void delete_node(cnode *node)
{
	//at head and single node in list
	if(node==head && head==tail)
	{
		head=NULL;
		tail=NULL;
	}
	//at tail of list
	else if(node->next==NULL)
	{
		tail=node->prev;
		tail->next=NULL;
	}
	//at head but not single node in list
	else if(node->prev==NULL)
	{
		head=node->next;
		head->prev=NULL;
	}
	else//in middle
	{
		node->next->prev=node->prev;
		node->prev->next=node->next;
	}
	rqst_in_cache--;
	present_cache_size=present_cache_size-node->node_size;
}

//insert a node in front of linkedlist
void insert_front(cnode *node)
{
	if(head==NULL)
	{
		head=node;
		tail=node;
		node->next=NULL;
		node->prev=NULL;
	}
	else
	{
		node->next=head;
		head->prev=node;
		node->prev=NULL;
		head=node;
	}
	rqst_in_cache++;
	present_cache_size=present_cache_size + node->node_size;
}

//function to delete least recently used request from list
void delete_LRU()
{
	delete_node(tail);
}
