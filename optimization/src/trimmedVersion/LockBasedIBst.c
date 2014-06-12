#include"header.h"
struct node* R=NULL;
struct node* S=NULL;
unsigned long numOfNodes;

static inline struct node* getAddress(struct node* p)
{
	return (struct node*)((uintptr_t) p & ~((uintptr_t) 3));
}

static inline bool isNull(struct node* p)
{
	return ((uintptr_t) p & 2) != 0;
}

static inline bool isLocked(struct node* p)
{
	return ((uintptr_t) p & 1) != 0;
}

static inline struct node* setLock(struct node* p)
{
	return (struct node*) ((uintptr_t) p | 1);
}

static inline struct node* unsetLock(struct node* p)
{
	return (struct node*) ((uintptr_t) p & ~((uintptr_t) 1));
}

static inline struct node* setNull(struct node* p)
{
	return (struct node*) ((uintptr_t) p | 2);
}

static inline bool CAS(struct node* parent, int which, struct node* oldChild, struct node* newChild)
{
	return(parent->child[which].compare_and_swap(newChild,oldChild) == oldChild);
}

static inline struct node* newLeafNode(unsigned long key)
{
  struct node* node = (struct node*) malloc(sizeof(struct node));
  node->key = key;
  node->child[LEFT] = setNull(NULL); 
  node->child[RIGHT] = setNull(NULL);
  return(node);
}

void createHeadNodes()
{
  R = newLeafNode(0);
  R->child[RIGHT] = newLeafNode(ULONG_MAX);
  S = R->child[RIGHT];
}

static inline bool lockEdge(struct node* parent, struct node* oldChild, int which, bool n)
{
	struct node* newChild;
  newChild = setLock(oldChild);
	if(n)
	{
		if(CAS(parent, which, setNull(oldChild), setNull(newChild)))
		{
			return true;
		}
	}
	else
	{
		if(CAS(parent, which, oldChild, newChild))
		{
			return true;
		}
	}
  return false;
}

static inline void unlockEdge(struct node* parent, int which)
{
  parent->child[which] = unsetLock(parent->child[which]);
}

static inline void seek(struct tArgs* t,unsigned long key)
{
	struct node* prev;
	struct node* curr;
	struct node* lastRNode;
	struct node* temp;
	struct node* address;
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
			temp = curr->child[which];
			n=isNull(temp);
			address=getAddress(temp);
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
			prev=curr; curr=address;			
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
	struct node* temp;
	bool n;
	
	prev=node; curr=rChild;
	while(true)
	{
		temp = curr->child[LEFT];
		n = isNull(temp); lChild = getAddress(temp);
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
	struct node* address;
	unsigned long nKey;
	int which;
	bool result;
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
		address = getAddress(node->child[which]);
		result = CAS(node,which,setNull(address),newNode);
		if(result)
		{
			t->isNewNodeAvailable = false;
			t->successfulInserts++;
			return(true);
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
	struct node* temp;
	unsigned long nKey;
	unsigned long pKey;
	int pWhich;
	bool ln;
	bool rn;
	bool lLock;
	bool rLock;
	
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
		parent = t->mySeekRecord->parent;
		pKey = parent->key;
		temp = node->child[LEFT];
		lChild = getAddress(temp); ln = isNull(temp); lLock = isLocked(temp);
		temp = node->child[RIGHT];
		rChild = getAddress(temp); rn = isNull(temp); rLock = isLocked(temp);
		pWhich = nKey < pKey ? LEFT: RIGHT;
		if(ln || rn) //simple delete
		{
			if(lockEdge(parent, node, pWhich, false))
			{
				if(lockEdge(node, lChild, LEFT, ln))
				{
					if(lockEdge(node, rChild, RIGHT, rn))
					{
						if(ln && rn) //00 case
						{
							parent->child[pWhich] = setNull(node);
						}
						else if(ln) //01 case
						{
							parent->child[pWhich] = rChild;
						}
						else //10 case
						{
							parent->child[pWhich] = lChild;
						}
						t->successfulDeletes++;
						t->simpleDeleteCount++;
						return true;
					}
					else
					{
						unlockEdge(parent, pWhich);
						unlockEdge(node, LEFT);
					}
				}
				else
				{
					unlockEdge(parent, pWhich);
				}
			}
		}
		else //complex delete
		{
			struct node* succNode;
			struct node* succParent;
			struct node* succNodeLChild;
			struct node* succNodeRChild;
			struct node* temp;
			bool isSplCase;
			bool srn;
			
			isSplCase = findSmallest(t,node,rChild);
			succNode = t->mySeekRecord->node;
			succParent = t->mySeekRecord->parent;
			succNodeLChild = getAddress(succNode->child[LEFT]);
			temp = succNode->child[RIGHT];
			srn = isNull(temp); succNodeRChild = getAddress(temp);
			if(lockEdge(succParent, succNode, isSplCase, false))
			{
				if(lockEdge(succNode, succNodeLChild, LEFT, true))
				{
					if(lockEdge(succNode, succNodeRChild, RIGHT, srn))
					{
						if(!isSplCase)
						{
							if(!lockEdge(node, rChild, RIGHT, false))
							{
								unlockEdge(succParent,isSplCase);
								unlockEdge(succNode, LEFT);
								unlockEdge(succNode, RIGHT);
								continue;
							}
						}
						node->key = succNode->key;
						if(srn)
						{
							succParent->child[isSplCase] = setNull(succNode);
						}
						else
						{
							succParent->child[isSplCase] = succNodeRChild;
						}
						if(!isSplCase)
						{
							unlockEdge(node, RIGHT);
						}
						t->successfulDeletes++;
						t->complexDeleteCount++;
						return true;
					}
					else
					{
						unlockEdge(succParent,isSplCase);
						unlockEdge(succNode, LEFT);
					}
				}
				else
				{
					unlockEdge(succParent,isSplCase);
				}
			}			
		}
		t->deleteRetries++;
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
  printKeysInOrder(getAddress(node)->child[LEFT]);
  printf("%lu\t",node->key);
  printKeysInOrder(getAddress(node)->child[RIGHT]);

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