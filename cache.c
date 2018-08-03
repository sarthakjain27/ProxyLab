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

cnode *head,*tail;
int rqst_in_cache;
volatile size_t present_cache_size;
volatile int readcnt;
sem_t mutex,w;

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
	Sem_init(&mutex,0,1); // to control number readcnt update when GET request comes
	Sem_init(&w,0,1); // to control access to cache when modification/reading is happening
}
