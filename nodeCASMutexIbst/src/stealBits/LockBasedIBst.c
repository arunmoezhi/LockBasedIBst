#include"header.h"
struct node* R=NULL;
struct node* S=NULL;
unsigned long numOfNodes;

static inline struct node* getAddress(struct node* p)
{
	return (struct node*)((uintptr_t) p & ~((uintptr_t) 1));
}

static inline bool isNull(struct node* p)
{
	return ((uintptr_t) p & 1) != 0;
}

static inline struct node* setNull(struct node* p)
{
	return (struct node*) ((uintptr_t) p | 1);
}

static inline bool lock(struct node* p)
{
	if(p->lockVar)
	{
		return false;
	}
	return(p->lockVar.compare_and_swap(true,false) == false);
}

static inline void unlock(struct node* p)
{
	p->lockVar = false;
}

static inline struct node* newLeafNode(unsigned long key)
{
  struct node* node = (struct node*) malloc(sizeof(struct node));
  node->key = key;
  node->child[LEFT] = setNull(NULL); 
  node->child[RIGHT] = setNull(NULL);
	node->lockVar=false;
  return(node);
}

void createHeadNodes()
{
  R = newLeafNode(0);
  R->child[RIGHT] = newLeafNode(ULONG_MAX);
  S = R->child[RIGHT];
}

static inline void seek(struct tArgs* t,unsigned long key)
{
	struct node* prev;
	struct node* curr;
	struct node* lastRNode;
	struct node* next;
	unsigned long lastRKey;
	unsigned long cKey;
	int which;
	bool done;
	bool n;
	
	while(true)
	{
		prev = R; curr = S;
		lastRNode = R; lastRKey = 0;
		done = false;
		while(true)
		{
			//read the key stored in the current node
			cKey = curr->key;
			if(key < cKey)
			{
				which = LEFT;
			}
			else if(key > cKey)
			{
				which = RIGHT;
			}
			else
			{
				//key found; stop the traversal
				done = true;
				break;
			}
			struct node* temp = curr->child[which];
			bool n = isNull(temp); next = getAddress(temp);
			if(n)
			{
				//null flag set; reached a leaf node
				if(lastRNode->key == lastRKey)
				{
					//key stored in the node at which the last right edge was traversed has not changed
					done = true;
				}
				break;
			}
			if(which == RIGHT)
			{
				//the next edge that will be traversed is the right edge; keep track of the current node and its key
				lastRNode = curr;
				lastRKey = cKey;
			}
			//traverse the next edge
			prev=curr; curr=next;			
		}
		if(done)
		{
			t->mySeekRecord->node = curr;
			t->mySeekRecord->parent = prev;		
			return;
		}
		t->readRetries++;
	}
}

static inline bool findSmallest(struct tArgs* t, struct node* node, struct node* rChild)
{
	//find the node with the smallest key in the subtree rooted at the right child
	struct node* prev;
	struct node* curr;
	struct node* lChild;
	
	prev=node; curr=rChild;
	while(true)
	{
		struct node* temp = curr->child[LEFT];
		bool n = isNull(temp); lChild = getAddress(temp);
		if(n)
		{
			break;
		}
		//traverse the next edge
		prev = curr; curr = lChild;
	}
	//initialize seek record and return
	t->mySeekRecord->node = curr;
	t->mySeekRecord->parent = prev;
	if(prev == node)
	{
		return true;
	}
	else
	{
		return false;
	}
}

unsigned long lookup(struct tArgs* t, unsigned long key)
{
  struct node* node;
	seek(t,key);
	node = t->mySeekRecord->node;
  if(key == node->key)
	{
		t->successfulReads++;
		return true;
	}
	else
	{
		t->unsuccessfulReads++;
		return false;
	}
}

bool insert(struct tArgs* t, unsigned long key)
{
	struct node* node;
	struct node* newNode;
	unsigned long nKey;
	int which;
	while(true)
	{
		seek(t,key);
		node = t->mySeekRecord->node;
		nKey = node->key;
		if(nKey == key)
		{
			t->unsuccessfulInserts++;
			return(false);
		}
		//create a new node and initialize its fields
		if(!t->isNewNodeAvailable)
		{
			t->newNode = newLeafNode(key);
			t->isNewNodeAvailable = true;
		}
		newNode = t->newNode;
		newNode->key = key;
		which = key<nKey ? LEFT:RIGHT;
		if(lock(node))
		{
			if(node->key == nKey && isNull(node->child[which]))
			{
				node->child[which] = newNode;
				t->isNewNodeAvailable = false;
				t->successfulInserts++;
				unlock(node);
				return true;
			}
			else
			{
				unlock(node);
			}
		}
		t->insertRetries++;
	}
}

bool remove(struct tArgs* t, unsigned long key)
{
	struct node* node;
	struct node* parent;
	struct node* lChild;
	struct node* rChild;
	unsigned long nKey;
	unsigned long pKey;
	int pWhich;
	
  while(true)
	{
		seek(t,key);
		node = t->mySeekRecord->node;
		nKey = node->key;
		if(key != nKey)
		{
			t->unsuccessfulDeletes++;
			return false;
		}
		if(lock(node))
		{
			if(node->key != nKey)
			{
				unlock(node);
				t->deleteRetries++;
				continue;
			}
			lChild = node->child[LEFT];
			rChild = node->child[RIGHT];
			if(isNull(lChild) || isNull(rChild)) //simple delete
			{
				parent = t->mySeekRecord->parent;
				if(lock(parent))
				{
					pKey = parent->key;
					pWhich = nKey < pKey ? LEFT: RIGHT;
					if(parent->child[pWhich] != node)
					{
						unlock(node);
						unlock(parent);
						t->deleteRetries++;
						continue;
					}
				}
				else
				{
					unlock(node);
					t->deleteRetries++;
					continue;
				}
				if(isNull(lChild) && isNull(rChild)) //00 case
				{
					parent->child[pWhich] = setNull(parent->child[pWhich]);
				}
				else if(isNull(lChild)) //01 case
				{
					parent->child[pWhich] = node->child[RIGHT];
				}
				else //10 case
				{
					parent->child[pWhich] = node->child[LEFT];
				}
				unlock(parent);
				t->successfulDeletes++;
				t->simpleDeleteCount++;
				return true;
			}
			else //complex delete
			{
				struct node* succNode;
				struct node* succParent;
				struct node* succNodeLChild;
				struct node* succNodeRChild;
				bool isSplCase;
				isSplCase = findSmallest(t,node,rChild);
				succNode = t->mySeekRecord->node;
				if(lock(succNode))
				{
					if(!isNull(succNode->child[LEFT]))
					{
						unlock(node);
						unlock(succNode);
						t->deleteRetries++;
						continue;
					}
				}
				else
				{
					unlock(node);
					t->deleteRetries++;
					continue;
				}
				succParent = t->mySeekRecord->parent;
				if(!isSplCase)
				{
					if(lock(succParent))
					{
						if(succParent->child[LEFT] != succNode)
						{
							unlock(node);
							unlock(succNode);
							unlock(succParent);
							t->deleteRetries++;
							continue;
						}
					}
					else
					{
						unlock(node);
						unlock(succNode);
						t->deleteRetries++;
						continue;
					}
				}
				node->key = succNode->key;
				succParent->child[isSplCase] = succNode->child[RIGHT];
				unlock(succParent);
				if(!isSplCase)
				{
					unlock(node);
				}
				t->successfulDeletes++;
				t->complexDeleteCount++;
				return true;
			}
		}
	}
}

void nodeCount(struct node* node)
{
  if(isNull(node))
  {
    return;
  }
  numOfNodes++;
	nodeCount(node->child[LEFT]);
  nodeCount(node->child[RIGHT]);
}

unsigned long size()
{
  numOfNodes=0;
  nodeCount(S->child[LEFT]);
  return numOfNodes;
}

void printKeysInOrder(struct node* node)
{
  if(isNull(node))
  {
    return;
  }
  printKeysInOrder(node->child[LEFT]);
  printf("%lu\t",node->key);
  printKeysInOrder(node->child[RIGHT]);

}

void printKeys()
{
  printKeysInOrder(R);
  printf("\n");
}

bool isValidBST(struct node* node, unsigned long min, unsigned long max)
{
  if(isNull(node))
  {
    return true;
  }
  if(node->key > min && node->key < max && isValidBST(node->child[LEFT],min,node->key) && isValidBST(node->child[RIGHT],node->key,max))
  {
    return true;
  }
  return false;
}

bool isValidTree()
{
  return(isValidBST(S->child[LEFT],0,ULONG_MAX));
}
