#include<stdio.h>
#include<pthread.h>
#include<stdlib.h>
#include<stdbool.h>
#include<limits.h>
#include<math.h>
#include<time.h>
#define __STDC_LIMIT_MACROS
#include<stdint.h>
#include<tbb/atomic.h>
#include<gsl/gsl_rng.h>
#include<gsl/gsl_randist.h>
#define UINTPTR_MAX_XOR_WITH_1 (uintptr_t) (UINTPTR_MAX ^ 1)
#define UINTPTR_MAX_XOR_WITH_3 (uintptr_t) (UINTPTR_MAX ^ 3)
//#define DEBUG_ON

struct node
{
  unsigned long key;
  tbb::atomic<struct node*> lChild;    //format <address,lockbit>
  tbb::atomic<struct node*> rChild;    //format <address,lockbit>
};

struct threadArgs
{
  int threadId;
  unsigned long lseed;
  unsigned int iseed;
  unsigned long readCount;
  unsigned long successfulReads;
  unsigned long unsuccessfulReads;
  unsigned long readRetries;
  unsigned long insertCount;
  unsigned long successfulInserts;
  unsigned long unsuccessfulInserts;
  unsigned long insertRetries;
  unsigned long deleteCount;
  unsigned long successfulDeletes;
  unsigned long unsuccessfulDeletes;
  unsigned long deleteRetries;
  unsigned long simpleDeleteCount;
  unsigned long complexDeleteCount;
  struct node* newNode;
  bool isNewNodeAvailable;
};

void createHeadNodes();
unsigned long lookup(struct threadArgs*, unsigned long);
bool insert(struct threadArgs*, unsigned long);
bool remove(struct threadArgs*, unsigned long);
unsigned long size();
void printKeys();
