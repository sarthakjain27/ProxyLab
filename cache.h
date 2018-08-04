/*
 * Prototype and definitions for cache.c
 */
#ifndef __CACHE_H__
#define __CACHE_H__

#define MAX_CACHE_SIZE (1024*1024)
#define MAX_OBJECT_SIZE (100*1024)

typedef struct cache_node{
	char *hostname;
	char *pathname;
	int port;
	char *response;
	size_t node_size;
	struct cache_node *next;
	struct cache_node *prev;
}cnode;

extern cnode *head,*tail;
extern int rqst_in_cache;
extern volatile size_t present_cache_size;
extern volatile int readcnt;
extern sem_t mutex,w;

void cache_initial();
cnode *check(char *host,char *uri,int def_port);
void delete_node(cnode *node);
void insert_front(cnode *node);
cnode *create_node(char *hostname, char *pathname, int port, char *response, size_t response_size);
void delete_LRU();
