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
