#include"header.h"
int NUM_OF_THREADS;
int findPercent;
int insertPercent;
int deletePercent;
unsigned long iterations;
unsigned long keyRange;
double* timeArray;
double MOPS;
volatile bool start=false;

struct timespec diff(timespec start, timespec end) //get difference in time in nano seconds
{
	struct timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) 
	{
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else 
	{
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
}
void *operateOnTree(void* tArgs)
{
  struct timespec s,e;
  int chooseOperation;
  unsigned long lseed;
  int threadId;
  struct threadArgs* tData = (struct threadArgs*) tArgs;
  threadId = tData->threadId;
  lseed = tData->lseed;
  const gsl_rng_type* T;
  gsl_rng* r;
  gsl_rng_env_setup();
  T = gsl_rng_default;
  r = gsl_rng_alloc(T);
  gsl_rng_set(r,lseed);
  while(!start)
  {
  }

  clock_gettime(CLOCK_REALTIME,&s);
  for(int i=0;i< (int)iterations;i++)
  {
    chooseOperation = gsl_rng_uniform(r)*100;
    if(chooseOperation < findPercent)
    {
      lookup(tData,gsl_rng_get(r)%keyRange + 1);
    }
    else if (chooseOperation < insertPercent)
    {
      insert(tData,gsl_rng_get(r)%keyRange + 1);
    }
    else
    {
      remove(tData,gsl_rng_get(r)%keyRange + 1);
    }
  }
  clock_gettime(CLOCK_REALTIME,&e);
  timeArray[threadId] = (double) (diff(s,e).tv_sec * 1000000000 + diff(s,e).tv_nsec)/1000;
  return NULL;
}

int main(int argc, char *argv[])
{
  struct threadArgs** tArgs;
  double totalTime=0;
  double avgTime=0;
	unsigned long lseed;
	//get run configuration from command line
  NUM_OF_THREADS = atoi(argv[1]);
  findPercent = atoi(argv[2]);
  insertPercent= findPercent + atoi(argv[3]);
  deletePercent = insertPercent + atoi(argv[4]);
  iterations = (unsigned long) pow(2,atoi(argv[5]));
  keyRange = (unsigned long) pow(2,atoi(argv[6]));
	lseed = (unsigned long) atol(argv[7]);
	
  timeArray = (double*)malloc(NUM_OF_THREADS * sizeof(double));
  tArgs = (struct threadArgs**) malloc(NUM_OF_THREADS * sizeof(struct threadArgs*)); 

  const gsl_rng_type* T;
  gsl_rng* r;
  gsl_rng_env_setup();
  T = gsl_rng_default;
  r = gsl_rng_alloc(T);
  gsl_rng_set(r,lseed);
	
  createHeadNodes(); //Initialize the tree. Must be called before doing any operations on the tree
  
  struct threadArgs* initialInsertArgs = (struct threadArgs*) malloc(sizeof(struct threadArgs));
  initialInsertArgs->successfulInserts=0;
  while(initialInsertArgs->successfulInserts < keyRange/2) //populate the tree with 50% of keys
  {
    insert(initialInsertArgs,gsl_rng_get(r)%keyRange + 1);
  }

  pthread_t threadArray[NUM_OF_THREADS];
  for(int i=0;i<NUM_OF_THREADS;i++)
  {
    tArgs[i] = (struct threadArgs*) malloc(sizeof(struct threadArgs));
    tArgs[i]->threadId = i;
    tArgs[i]->lseed = gsl_rng_get(r);
    tArgs[i]->readCount=0;
    tArgs[i]->successfulReads=0;
    tArgs[i]->unsuccessfulReads=0;
    tArgs[i]->readRetries=0;
    tArgs[i]->insertCount=0;
    tArgs[i]->successfulInserts=0;
    tArgs[i]->unsuccessfulInserts=0;
    tArgs[i]->insertRetries=0;
    tArgs[i]->deleteCount=0;
    tArgs[i]->successfulDeletes=0;
    tArgs[i]->unsuccessfulDeletes=0;
    tArgs[i]->deleteRetries=0;
    tArgs[i]->simpleDeleteCount=0;
    tArgs[i]->complexDeleteCount=0;
    tArgs[i]->newNode=NULL;
    tArgs[i]->isNewNodeAvailable=false;
  }

  for(int i=0;i<NUM_OF_THREADS;i++)
  {
    pthread_create(&threadArray[i], NULL, operateOnTree, (void*) tArgs[i] );
  }
  start=true; //start operations
  for(int i=0;i<NUM_OF_THREADS;i++)
  {
    pthread_join(threadArray[i], NULL);
  }
  for(int i=0;i<NUM_OF_THREADS;i++)
  {
    totalTime += timeArray[i];
  }
  avgTime = totalTime/NUM_OF_THREADS;
  MOPS = iterations*NUM_OF_THREADS/(avgTime);
  printf("k%d;%d-%d-%d;%d;%ld;%.2f\n",atoi(argv[6]),findPercent,(insertPercent-findPercent),(deletePercent-insertPercent),NUM_OF_THREADS,size(),MOPS);

  unsigned long totalReadCount=0;
  unsigned long totalSuccessfulReads=0;
  unsigned long totalUnsuccessfulReads=0;
  unsigned long totalReadRetries=0;
  unsigned long totalInsertCount=0;
  unsigned long totalSuccessfulInserts=0;
  unsigned long totalUnsuccessfulInserts=0;
  unsigned long totalInsertRetries=0;
  unsigned long totalDeleteCount=0;
  unsigned long totalSuccessfulDeletes=0;
  unsigned long totalUnsuccessfulDeletes=0;
  unsigned long totalDeleteRetries=0;
  unsigned long totalSimpleDeleteCount=0;
  unsigned long totalComplexDeleteCount=0;
 
  for(int i=0;i<NUM_OF_THREADS;i++)
  {
    totalReadCount += tArgs[i]->readCount;
    totalSuccessfulReads += tArgs[i]->successfulReads;
    totalUnsuccessfulReads += tArgs[i]->unsuccessfulReads;
    totalReadRetries += tArgs[i]->readRetries;

    totalInsertCount += tArgs[i]->insertCount;
    totalSuccessfulInserts += tArgs[i]->successfulInserts;
    totalUnsuccessfulInserts += tArgs[i]->unsuccessfulInserts;
    totalInsertRetries += tArgs[i]->insertRetries;
    totalDeleteCount += tArgs[i]->deleteCount;
    totalSuccessfulDeletes += tArgs[i]->successfulDeletes;
    totalUnsuccessfulDeletes += tArgs[i]->unsuccessfulDeletes;
    totalDeleteRetries += tArgs[i]->deleteRetries;
    totalSimpleDeleteCount += tArgs[i]->simpleDeleteCount;
    totalComplexDeleteCount += tArgs[i]->complexDeleteCount;
  }
	#ifdef STATS_ON
  printf("==========================================================================\n");
  printf("Detailed Stats\n");
  printf("operation\t     count\tsuccessful    unsuccessful\t   retries\t %% retries\t       simpleDel\t\t complexDel\n");
  printf("Read     \t%10lu\t%10lu\t%10lu\t%10lu\t%10.1f\n",totalReadCount,totalSuccessfulReads,totalUnsuccessfulReads,totalReadRetries,(totalReadRetries * 100.0/totalReadCount));
  printf("Insert   \t%10lu\t%10lu\t%10lu\t%10lu\t%10.1f\n",totalInsertCount,totalSuccessfulInserts,totalUnsuccessfulInserts,totalInsertRetries,(totalInsertRetries * 100.0/totalInsertCount));
  printf("Delete   \t%10lu\t%10lu\t%10lu\t%10lu\t%10.1f\t%10lu (%.1f)\t%13lu (%.1f)\n",totalDeleteCount,totalSuccessfulDeletes,totalUnsuccessfulDeletes,totalDeleteRetries,(totalDeleteRetries * 100.0/totalDeleteCount),totalSimpleDeleteCount,(totalSimpleDeleteCount*100.0/totalSuccessfulDeletes),totalComplexDeleteCount,(totalComplexDeleteCount*100.0/totalSuccessfulDeletes));
 	printf("==========================================================================\n");
	#endif
	assert(isValidTree());
	assert(totalReadCount==totalSuccessfulReads+totalUnsuccessfulReads);
	assert(totalInsertCount==totalSuccessfulInserts+totalUnsuccessfulInserts);
  assert(totalDeleteCount==totalSuccessfulDeletes+totalUnsuccessfulDeletes && (totalSuccessfulDeletes == totalSimpleDeleteCount + totalComplexDeleteCount));
  assert(initialInsertArgs->successfulInserts + totalSuccessfulInserts - totalSuccessfulDeletes == size());
  
	pthread_exit(NULL);
}
