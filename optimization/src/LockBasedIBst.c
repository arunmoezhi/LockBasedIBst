#include"header.h"

struct node
{
  unsigned long key;
  tbb::atomic<struct node*> lChild;    //format <address,lockbit>
  tbb::atomic<struct node*> rChild;    //format <address,lockbit>
};

struct node* grandParentHead=NULL;
struct node* parentHead=NULL;
unsigned long numOfNodes;

static inline struct node* newLeafNode(unsigned long key)
{
  struct node* node = (struct node*) malloc(sizeof(struct node));
  node->key = key;
  node->lChild = NULL; 
  node->rChild = NULL;
  return(node);
}

void createHeadNodes()
{
  grandParentHead = newLeafNode(ULONG_MAX);
  grandParentHead->lChild = newLeafNode(ULONG_MAX);
  parentHead = grandParentHead->lChild;
}

static inline bool getLockBit(struct node* p)
{
  return (uintptr_t) p & 1;
}

//#define getAddress(p) ((struct node*) ((uintptr_t) (struct node*) (p) & UINTPTR_MAX_XOR_WITH_1))

static inline struct node* getAddress(struct node* p)
{
  return (struct node*)((uintptr_t) p & UINTPTR_MAX_XOR_WITH_1);
}

static inline struct node* setLockBit(struct node* p)
{
  return((struct node*) (((uintptr_t) p & UINTPTR_MAX_XOR_WITH_1) | 1));
}

static inline struct node* unsetLockBit(struct node* p)
{
  return((struct node*) (((uintptr_t) p & UINTPTR_MAX_XOR_WITH_1) | 0));
}

static inline bool lockLChild(struct node* parent)
{
  struct node* lChild;
  struct node* lockedLChild;

  lChild = parent->lChild;
  if(getLockBit(lChild))
  {
    return false;
  }
  lockedLChild = setLockBit(lChild);

  if(parent->lChild.compare_and_swap(lockedLChild,lChild) == lChild)
  {
    return true;
  }
  return false;
}

static inline bool lockRChild(struct node* parent)
{
  struct node* rChild;
  struct node* lockedRChild;

  rChild = parent->rChild;
  if(getLockBit(rChild))
  {
    return false;
  }
  lockedRChild = setLockBit(rChild);

  if(parent->rChild.compare_and_swap(lockedRChild,rChild) == rChild)
  {
    return true;
  }
  return false;
}

static inline void unlockLChild(struct node* parent)
{
  parent->lChild = unsetLockBit(parent->lChild);
}

static inline void unlockRChild(struct node* parent)
{
  parent->rChild = unsetLockBit(parent->rChild);
}

unsigned long lookup(struct threadArgs* tData, unsigned long target)
{
  struct node* node;
  unsigned long lastRightKey;
  struct node* lastRightNode;
  tData->readCount++;
  while(true)
  {
    node = grandParentHead;
    lastRightKey = node->key;
    lastRightNode = node;
    while( node != NULL ) //Loop until a child of a leaf node which is null is reached
    {
      if(target < node->key)
      {
        node = getAddress(node->lChild);
      }
      else if (target > node->key)
      {
        lastRightKey = node->key;
        lastRightNode = node;
        node = getAddress(node->rChild);
      }
      else
      {
        tData->successfulReads++;
        return(1);
      }
    }
    if(lastRightNode->key == lastRightKey)
    {
      tData->unsuccessfulReads++;
      return(0);
    }
    tData->readRetries++;
  }
}

bool insert(struct threadArgs* tData, unsigned long insertKey)
{
  struct node* pnode;
  struct node* node;
  struct node* replaceNode;
  unsigned long lastRightKey;
  struct node* lastRightNode;
  
  tData->insertCount++;
  
  while(true)
  {
    while(true)
    {
      pnode = grandParentHead;
      node = parentHead;
      replaceNode = NULL;
      lastRightKey = node->key;
      lastRightNode = node;

      while( node != NULL ) //Loop until a child of a leaf node which is null is reached
      {
        if(insertKey < node->key)
        {
          pnode = node;
          node = getAddress(node->lChild);
        }
        else if (insertKey > node->key)
        {
          lastRightKey = node->key;
          lastRightNode = node;
          pnode = node;
          node = getAddress(node->rChild);
        }
        else
        {
          #ifdef DEBUG_ON
          printf("Failed I%lu\t%lu\n",insertKey,getAddress(parentHead->lChild)->key);
          #endif
          tData->unsuccessfulInserts++;
          return(false);
        }
      }
      if(lastRightNode->key == lastRightKey)
      {
        break;  
      }
    }

    if(!getLockBit(node)) //If locked restart
    {
      replaceNode = newLeafNode(insertKey);

      if(insertKey < pnode->key) //left case
      {
        if(pnode->lChild.compare_and_swap(replaceNode,node) == node)
        {
          tData->successfulInserts++;
          #ifdef DEBUG_ON
          printf("Success I%lu\t%lu\n",insertKey,getAddress(parentHead->lChild)->key);
          #endif
          return(true);
        }
      }
      else                      //right case
      {
        if(pnode->rChild.compare_and_swap(replaceNode,node) == node)
        {
          tData->successfulInserts++;
          #ifdef DEBUG_ON
          printf("Success I%lu\t%lu\n",insertKey,getAddress(parentHead->lChild)->key);
          #endif
          return(true);
        }
      }
      free(replaceNode);
    }
    tData->insertRetries++;
  }
}

bool remove(struct threadArgs* tData, unsigned long deleteKey)
{
  struct node* pnode;
  struct node* node;
  unsigned long lastRightKey;
  struct node* lastRightNode;
  bool keyFound;
  tData->deleteCount++;
  
  while(true)
  {
    while(true)
    {
      pnode = grandParentHead;
      node = parentHead;
      lastRightKey = node->key;
      lastRightNode = node;
      keyFound = false;

      while( node != NULL ) //Loop until a child of a leaf node which is null is reached
      {
        if(deleteKey < node->key)
        {
          pnode = node;
          node = getAddress(node->lChild);
        }
        else if (deleteKey > node->key)
        {
          lastRightKey = node->key;
          lastRightNode = node;
          pnode = node;
          node = getAddress(node->rChild);
        }
        else //key to be deleted is found
        {
          keyFound = true;
          break;
        }
      }
      if(keyFound)
      {
        break;
      }
      if(lastRightNode->key == lastRightKey)
      {
        break;
      }
    }
    if(node != NULL)
    {
      if(deleteKey == node->key)
      {
				if(getAddress(pnode->lChild) == node) //left case
				{
					if(getAddress(node->lChild) == NULL || getAddress(node->rChild) == NULL ) //possible simple delete
					{
						if(lockLChild(pnode))
						{
							if(lockLChild(node))
							{
								if(lockRChild(node)) //3 locks are obtained
								{
									if(getAddress(node->lChild) == NULL)
									{
										pnode->lChild = getAddress(node->rChild);
										tData->successfulDeletes++;
                    tData->simpleDeleteCount++;
										return(true);
									}
									else if(getAddress(node->rChild) == NULL)
									{
										pnode->lChild = getAddress(node->lChild);
										tData->successfulDeletes++;
                    tData->simpleDeleteCount++;
										return(true);
									}
									else //looked like a simple delete but became complex delete after obtaining locks
									{
										struct node* rpnode;
										struct node* rnode;
										struct node* lcrnode;
										rpnode = node;
										rnode = getAddress(node->rChild);
										lcrnode = getAddress(rnode->lChild);
										if(lcrnode != NULL)
										{
											while(lcrnode != NULL)
											{
												rpnode = rnode;
												rnode = lcrnode;
												lcrnode = getAddress(lcrnode->lChild);
											}
											if(lockLChild(rpnode))
											{
                        if(lockLChild(rnode))
                        {
												  if(lockRChild(rnode))
												  {
                            if(getAddress(rnode->lChild) == NULL)
                            {
												  	  node->key = rnode->key;
												  	  rpnode->lChild = getAddress(rnode->rChild);
												  	  unlockLChild(pnode);
												  	  unlockLChild(node);
												  	  unlockRChild(node);
												  	  #ifdef DEBUG_ON
												  	  printf("Success CD%lu\t%lu\n",deleteKey,getAddress(parentHead->lChild)->key);
												  	  #endif
                              tData->successfulDeletes++;
                              tData->complexDeleteCount++;
												  	  return(true);
                            }
                            else
                            {
												  	  unlockLChild(pnode);
												  	  unlockLChild(node);
												  	  unlockRChild(node);
												  	  unlockLChild(rpnode);
                              unlockLChild(rnode);
                              unlockRChild(rnode);  
                            }
												  }
												  else
												  {
												  	unlockLChild(pnode);
												  	unlockLChild(node);
												  	unlockRChild(node);
												  	unlockLChild(rpnode);
                            unlockLChild(rnode);
												  }
                        }
                        else
                        {
												  unlockLChild(pnode);
												  unlockLChild(node);
												  unlockRChild(node);
												  unlockLChild(rpnode);
                        }
											}
											else
											{
												unlockLChild(pnode);
												unlockLChild(node);
												unlockRChild(node);
											}
										}
										else
										{
                      if(lockLChild(rnode))
                      {
											  if(lockRChild(rnode))
											  {
                          if(getAddress(rnode->lChild) == NULL)
                          {
											  	  node->key = rnode->key;
											  	  node->rChild = getAddress(rnode->rChild);
											  	  unlockLChild(pnode);
											  	  unlockLChild(node);
											  	  #ifdef DEBUG_ON
											  	  printf("Success CD%lu\t%lu\n",deleteKey,getAddress(parentHead->lChild)->key);
											  	  #endif
                            tData->successfulDeletes++;
                            tData->complexDeleteCount++;
											  	  return(true);
                          }
                          else
                          {
											  	  unlockLChild(pnode);
											  	  unlockLChild(node);
											  	  unlockRChild(node);
                            unlockLChild(rnode);
                            unlockRChild(rnode);
                          }
											  }
											  else
											  {
											  	unlockLChild(pnode);
											  	unlockLChild(node);
											  	unlockRChild(node);
                          unlockLChild(rnode);
											  }
                      }
                      else
                      {
											  unlockLChild(pnode);
											  unlockLChild(node);
											  unlockRChild(node);
                      }
										}	
									}
								}
								else
								{
									unlockLChild(pnode);
									unlockLChild(node);
								}
							}
							else
							{
								unlockLChild(pnode);
							}
						}
					}
					else //possible complex delete. 
					{
						if(lockRChild(node)) //lock only the rChild of node.
						{
							if(getAddress(node->rChild) != NULL) //validate if rChild is still non-NULL
							{
								struct node* rpnode;
								struct node* rnode;
								struct node* lcrnode;
								rpnode = node;
								rnode = getAddress(node->rChild);
								lcrnode = getAddress(rnode->lChild);
								if(lcrnode != NULL)
								{
									while(lcrnode != NULL)
									{
										rpnode = rnode;
										rnode = lcrnode;
										lcrnode = getAddress(lcrnode->lChild);
									}
									if(lockLChild(rpnode))
									{
                    if(lockLChild(rnode))
                    {
										  if(lockRChild(rnode))
										  {
                        if(getAddress(rnode->lChild) == NULL)
                        {
										  	  node->key = rnode->key;
										  	  rpnode->lChild = getAddress(rnode->rChild);
										  	  unlockRChild(node);
										  	  #ifdef DEBUG_ON
										  	  printf("Success CD%lu\t%lu\n",deleteKey,getAddress(parentHead->lChild)->key);
										  	  #endif
                          tData->successfulDeletes++;
                          tData->complexDeleteCount++;
										  	  return(true);
                        }
                        else
                        {
										  	  unlockRChild(node);
										  	  unlockLChild(rpnode);
                          unlockLChild(rnode);
                          unlockRChild(rnode);  
                        }
										  }
										  else
										  {
										  	unlockRChild(node);
										  	unlockLChild(rpnode);
                        unlockLChild(rnode);
										  }
                    }
                    else
                    {
										  unlockRChild(node);
										  unlockLChild(rpnode);
                    }
									}
									else
									{
										unlockRChild(node);
									}
								}
								else
								{
                  if(lockLChild(rnode))
                  {
									  if(lockRChild(rnode))
									  {
                      if(getAddress(rnode->lChild) == NULL)
                      {
									  	  node->key = rnode->key;
									  	  node->rChild = getAddress(rnode->rChild);
									  	  #ifdef DEBUG_ON
									  	  printf("Success CD%lu\t%lu\n",deleteKey,getAddress(parentHead->lChild)->key);
									  	  #endif
                        tData->successfulDeletes++;
                        tData->complexDeleteCount++;
									  	  return(true);
                      }
                      else
                      {
									  	  unlockRChild(node);
                        unlockLChild(rnode);
                        unlockRChild(rnode);
                      }
									  }
									  else
									  {
									  	unlockRChild(node);
                      unlockLChild(rnode);
									  }
                  }
                  else
                  {
									  unlockRChild(node);
                  }
								}
							}
							else //rChild has become NULL. So it becomes a simple delete. Obtain locks on parent and left child
							{
								if(lockLChild(pnode))
								{
									if(lockLChild(node))
									{
										pnode->lChild = getAddress(node->lChild);
										tData->successfulDeletes++;
										tData->simpleDeleteCount++;
										return(true);
									}
									else
									{
										unlockRChild(node);
										unlockLChild(pnode);
									}
								}
								else
								{
									unlockRChild(node);
								}
							}
						}
					}
				}
				else //right case
				{
					if(getAddress(node->lChild) == NULL || getAddress(node->rChild) == NULL ) //possible simple delete
					{
						if(lockRChild(pnode))
						{
							if(lockLChild(node))
							{
								if(lockRChild(node)) //3 locks are obtained
								{
									if(getAddress(node->lChild) == NULL)
									{
										pnode->rChild = getAddress(node->rChild);
										tData->successfulDeletes++;
                    tData->simpleDeleteCount++;
										return(true);
									}
									else if(getAddress(node->rChild) == NULL)
									{
										pnode->rChild = getAddress(node->lChild);
										tData->successfulDeletes++;
                    tData->simpleDeleteCount++;
										return(true);
									}
									else //looked like a simple delete but became complex delete after obtaining locks
									{
										struct node* rpnode;
										struct node* rnode;
										struct node* lcrnode;
										rpnode = node;
										rnode = getAddress(node->rChild);
										lcrnode = getAddress(rnode->lChild);
										if(lcrnode != NULL)
										{
											while(lcrnode != NULL)
											{
												rpnode = rnode;
												rnode = lcrnode;
												lcrnode = getAddress(lcrnode->lChild);
											}
											if(lockLChild(rpnode))
											{
                        if(lockLChild(rnode))
                        {
												  if(lockRChild(rnode))
												  {
                            if(getAddress(rnode->lChild) == NULL)
                            {
												  	  node->key = rnode->key;
												  	  rpnode->lChild = getAddress(rnode->rChild);
												  	  unlockRChild(pnode);
												  	  unlockLChild(node);
												  	  unlockRChild(node);
												  	  #ifdef DEBUG_ON
												  	  printf("Success CD%lu\t%lu\n",deleteKey,getAddress(parentHead->lChild)->key);
												  	  #endif
                              tData->successfulDeletes++;
                              tData->complexDeleteCount++;
												  	  return(true);
                            }
                            else
                            {
												  	  unlockRChild(pnode);
												  	  unlockLChild(node);
												  	  unlockRChild(node);
												  	  unlockLChild(rpnode);
                              unlockLChild(rnode);
                              unlockRChild(rnode);  
                            }
												  }
												  else
												  {
												  	unlockRChild(pnode);
												  	unlockLChild(node);
												  	unlockRChild(node);
												  	unlockLChild(rpnode);
                            unlockLChild(rnode);
												  }
                        }
                        else
                        {
												  unlockRChild(pnode);
												  unlockLChild(node);
												  unlockRChild(node);
												  unlockLChild(rpnode);
                        }
											}
											else
											{
												unlockRChild(pnode);
												unlockLChild(node);
												unlockRChild(node);
											}
										}
										else
										{
                      if(lockLChild(rnode))
                      {
											  if(lockRChild(rnode))
											  {
                          if(getAddress(rnode->lChild) == NULL)
                          {
											  	  node->key = rnode->key;
											  	  node->rChild = getAddress(rnode->rChild);
											  	  unlockRChild(pnode);
											  	  unlockLChild(node);
											  	  #ifdef DEBUG_ON
											  	  printf("Success CD%lu\t%lu\n",deleteKey,getAddress(parentHead->lChild)->key);
											  	  #endif
                            tData->successfulDeletes++;
                            tData->complexDeleteCount++;
											  	  return(true);
                          }
                          else
                          {
											  	  unlockRChild(pnode);
											  	  unlockLChild(node);
											  	  unlockRChild(node);
                            unlockLChild(rnode);
                            unlockRChild(rnode);
                          }
											  }
											  else
											  {
											  	unlockRChild(pnode);
											  	unlockLChild(node);
											  	unlockRChild(node);
                          unlockLChild(rnode);
											  }
                      }
                      else
                      {
											  unlockRChild(pnode);
											  unlockLChild(node);
											  unlockRChild(node);
                      }
										}	
									}
								}
								else
								{
									unlockRChild(pnode);
									unlockLChild(node);
								}
							}
							else
							{
								unlockRChild(pnode);
							}
						}
					}
					else //possible complex delete. 
					{
						if(lockRChild(node)) //lock only the rChild of node.
						{
							if(getAddress(node->rChild) != NULL) //validate if rChild is still non-NULL
							{
								struct node* rpnode;
								struct node* rnode;
								struct node* lcrnode;
								rpnode = node;
								rnode = getAddress(node->rChild);
								lcrnode = getAddress(rnode->lChild);
								if(lcrnode != NULL)
								{
									while(lcrnode != NULL)
									{
										rpnode = rnode;
										rnode = lcrnode;
										lcrnode = getAddress(lcrnode->lChild);
									}
									if(lockLChild(rpnode))
									{
                    if(lockLChild(rnode))
                    {
										  if(lockRChild(rnode))
										  {
                        if(getAddress(rnode->lChild) == NULL)
                        {
										  	  node->key = rnode->key;
										  	  rpnode->lChild = getAddress(rnode->rChild);
										  	  unlockRChild(node);
										  	  #ifdef DEBUG_ON
										  	  printf("Success CD%lu\t%lu\n",deleteKey,getAddress(parentHead->lChild)->key);
										  	  #endif
                          tData->successfulDeletes++;
                          tData->complexDeleteCount++;
										  	  return(true);
                        }
                        else
                        {
										  	  unlockRChild(node);
										  	  unlockLChild(rpnode);
                          unlockLChild(rnode);
                          unlockRChild(rnode);  
                        }
										  }
										  else
										  {
										  	unlockRChild(node);
										  	unlockLChild(rpnode);
                        unlockLChild(rnode);
										  }
                    }
                    else
                    {
										  unlockRChild(node);
										  unlockLChild(rpnode);
                    }
									}
									else
									{
										unlockRChild(node);
									}
								}
								else
								{
                  if(lockLChild(rnode))
                  {
									  if(lockRChild(rnode))
									  {
                      if(getAddress(rnode->lChild) == NULL)
                      {
									  	  node->key = rnode->key;
									  	  node->rChild = getAddress(rnode->rChild);
									  	  #ifdef DEBUG_ON
									  	  printf("Success CD%lu\t%lu\n",deleteKey,getAddress(parentHead->lChild)->key);
									  	  #endif
                        tData->successfulDeletes++;
                        tData->complexDeleteCount++;
									  	  return(true);
                      }
                      else
                      {
									  	  unlockRChild(node);
                        unlockLChild(rnode);
                        unlockRChild(rnode);
                      }
									  }
									  else
									  {
									  	unlockRChild(node);
                      unlockLChild(rnode);
									  }
                  }
                  else
                  {
									  unlockRChild(node);
                  }
								}
							}
							else //rChild has become NULL. So it becomes a simple delete. Obtain locks on parent and left child
							{
								if(lockRChild(pnode))
								{
									if(lockLChild(node))
									{
										pnode->rChild = getAddress(node->lChild);
										tData->successfulDeletes++;
										tData->simpleDeleteCount++;
										return(true);
									}
									else
									{
										unlockRChild(node);
										unlockRChild(pnode);
									}
								}
								else
								{
									unlockRChild(node);
								}
							}
						}
					}
				}
      }
      else //if(deleteKey == node->key)
      {
        #ifdef DEBUG_ON
        printf("Failed D%lu\t%lu\n",deleteKey,getAddress(parentHead->lChild)->key);
        #endif
        tData->unsuccessfulDeletes++;
        return(false);
      }
    }
    else // for if(node != NULL)
    {
      #ifdef DEBUG_ON
      printf("Failed D%lu\t%lu\n",deleteKey,getAddress(parentHead->lChild)->key);
      #endif
      tData->unsuccessfulDeletes++;
      return(false);
    } 
    tData->deleteRetries++;
  } // end of infinite while loop
}

void nodeCount(struct node* node)
{
  if(node == NULL)
  {
    return;
  }
  numOfNodes++;
  nodeCount(node->lChild);
  nodeCount(node->rChild);
}

unsigned long size()
{
  numOfNodes=0;
  nodeCount(parentHead->lChild);
  return numOfNodes;
}

void printKeysInOrder(struct node* node)
{
  if(node == NULL)
  {
    return;
  }
  printKeysInOrder(node->lChild);
  printf("%lu\t",node->key);
  printKeysInOrder(node->rChild);

}

void printKeys()
{
  printKeysInOrder(parentHead);
  printf("\n");
}

