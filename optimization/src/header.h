#include<stdio.h>
#include<pthread.h>
#include<stdlib.h>
#include<stdbool.h>
#include<math.h>
#include<time.h>
#include<stdint.h>
#include<tbb/atomic.h>
#include<gsl/gsl_rng.h>
#include<gsl/gsl_randist.h>
#include<assert.h>
//#define STATS_ON

struct node
{
  unsigned long key;
  tbb::atomic<struct node*> lChild;    //format <address, nullbit, lockbit>
  tbb::atomic<struct node*> rChild;    //format <address, nullbit, lockbit>
};

struct threadArgs
{
  int threadId;
  unsigned long lseed;
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
bool isValidTree();
