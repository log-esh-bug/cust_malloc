#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<unistd.h>
#include<string.h>

#define MEM_BLOCK_SIZE (1024*1024)
#define total_size (1024*1024*1024)
#define MAX_BLOCKS total_size/MEM_BLOCK_SIZE
#define H_size (sizeof(int))
#define CHUNK_MIN_SIZE 16
#define SIZE_PADDER 8 

#define ARRAY_SIZE 1000000
#define tempMaxChunkSize 100
#define tempString "The truth is, there are lots of equally iconic Dickinson poems, so consider this a stand-in for them all."

typedef struct block{
    struct block* nextBlock;
    void* data;
    void* currptr;
}Block;

typedef struct mc{
    Block* blockList;
    int usedblocks;
    void* freePtr;
}MemoryContext;

MemoryContext* createMemoryContext(){
    MemoryContext* mc=(MemoryContext*)malloc(sizeof(MemoryContext));
    if (mc == NULL) {
        printf("Cannot create Memory context!\n");
        return NULL;
    }
    mc->blockList = NULL;
    mc->usedblocks = 0;
    mc->freePtr=NULL;
    return mc;
}

void setHeader(void* ptr,int size,int isFree){
    if(ptr==NULL)return;
    int h = size + isFree;
    *((int*)ptr-1)=h;
}

int isFree(void* ptr){
    return (*((int*)ptr-1)) & 1;
}

int extractsize(void* ptr){
    int a = *((int*)ptr);
    if(a&1) return a-1;
    return a;
}

int getSize(void* ptr){
    return extractsize(ptr-H_size);
}

int pad_size(int size){
    if(size<CHUNK_MIN_SIZE)return CHUNK_MIN_SIZE;
    if(size % SIZE_PADDER ==0)return size;
    return size +(SIZE_PADDER - size % SIZE_PADDER);
}

void removeFree(void* ptr,MemoryContext* mc){
    void* prev =*(void**)ptr;
    void* next=*((void**)ptr+1);
    // printf("prev: %p next: %p\n",prev,next);

    if(prev!=NULL && next!=NULL){
        *((void**)prev+1)=next;
        *(void**)next=prev;
    }
    else if(prev==NULL){
        if(next)
            *(void**)next=NULL;
        mc->freePtr=next;
    }
    else{
        *((void**)prev+1)=NULL;
    }
    *(void**)ptr=NULL;
    *((void**)ptr+1)=NULL;
}

void addFree(void* ptr,MemoryContext* mc){
    if(mc->freePtr==NULL ) {
        *(void**)ptr = NULL;
        *((void**)ptr+1) = NULL;
        mc->freePtr = ptr;
    }
    else{
        *(void**)ptr=NULL;
        *((void**)ptr+1)=mc->freePtr;
        *((void**)mc->freePtr)=ptr;
        mc->freePtr=ptr;
    } 
}

void* memoryAlloc(MemoryContext* mc,int size){
    size = pad_size(size);
    
    void* ptr=mc->freePtr;
    while(ptr){
        int nodeSize=getSize(ptr);
        if(nodeSize >= size){
            removeFree(ptr,mc);
            if (nodeSize >= size + H_size + CHUNK_MIN_SIZE) {
                void* newPtr=(void*)ptr+size + H_size;
                int newSize=nodeSize-size-H_size;
                setHeader(newPtr,newSize,1);
                setHeader(ptr,size,0);
                addFree(newPtr,mc);
            }
            else{
                setHeader(ptr,nodeSize,0);
            }
            return ptr;
        }
        ptr = *((void**) ptr+1);
    }

    Block* block=mc->blockList;
    while(block){
        ptr = block->currptr + H_size;
        if(ptr + size <= (block->data+MEM_BLOCK_SIZE)){
            setHeader(ptr,size,0);
            block->currptr=ptr+size ;
            // *(void**)(block->currptr+H_size) = NULL;
            return ptr;
        }
        block=block->nextBlock;
    }

    if(mc->usedblocks >= MAX_BLOCKS){
        return NULL;
    }
    block=(Block*)malloc(sizeof(Block));
    if(block==NULL){
        return NULL;
    }
    block->data=malloc(MEM_BLOCK_SIZE);

    if(block->data == NULL){
        free(block);
        printf("Unable to allocate a new Bock!\n");
        return NULL;
    }


    block->currptr=block->data;

    block->nextBlock=mc->blockList ;
    mc->blockList=block;
    mc->usedblocks++;

    void* p=block->currptr+H_size;
    setHeader(p,size,0);
    block->currptr = p+ size ;
    // *(void**)(block->currptr+H_size) = NULL;
    return p;
}



void memFree(void* ptr,MemoryContext* mc){

    
    
    int size= getSize(ptr);
    void* nextPtr= ptr + size + H_size;
    // printf("1 ");

    memset(ptr,'0',size);

    int totSize=size;

    while(isFree(nextPtr)){
        removeFree(nextPtr,mc);
        totSize+=getSize(nextPtr)+H_size;
        nextPtr = nextPtr +getSize(nextPtr) + H_size;
        if(nextPtr ==NULL) break;
    }

    addFree(ptr,mc);

    setHeader(ptr,totSize,1);  
}

void* memoryRealloc(void* ptr,int size,MemoryContext* mc){
    if(isFree(ptr)){
        printf("Memory is not allocated.\n");
        return NULL;
    }


    int nodeSize=getSize(ptr);

    size=pad_size(size);

    if(size == nodeSize){
        return ptr;
    }

    void* nextPtr = (void*)(ptr + nodeSize + H_size);
    void* newNext=NULL;
    int newNextSize;

    if(isFree(nextPtr)){
        removeFree(nextPtr,mc);
        if(size < nodeSize){
            setHeader(ptr,size,0);
            int extraSpace = nodeSize - size;
            newNext = ptr + size + H_size;
            newNextSize = getSize(nextPtr) + extraSpace;
            setHeader(newNext,newNextSize,1);
            addFree(newNext,mc);
            return ptr;
        }
        else if(size > nodeSize){
            int neededSize = size - nodeSize;
            if(neededSize <= getSize(nextPtr)){
                newNextSize = getSize(ptr) + getSize(nextPtr) - size;
                newNext = ptr + size + H_size;
                setHeader(ptr,size,0);
                setHeader(newNext,newNextSize,1);
                addFree(newNext,mc);
                return ptr;
            }
        }
    }

    else if(size + H_size + CHUNK_MIN_SIZE < nodeSize ){
        
        setHeader(ptr,size,0);

        newNext = (void*)(ptr + size + H_size);
        newNextSize =nodeSize-(size + H_size);

        setHeader(newNext,newNextSize,1);
        addFree(newNext,mc);
        return ptr;
    }
    else if(size < nodeSize){
        return ptr;
    }

    memFree(ptr,mc);
    return memoryAlloc(mc,size);
}

void destroyMemoryContext(MemoryContext* mc) {
    if (mc == NULL) {
        printf("Memory context is not created!\n");
        return;
    }
    Block* block = mc->blockList;
    while (block!=NULL) {
        Block* temp=block;
        block=block->nextBlock;
        free(temp->data);
        free(temp);
    }
    free(mc);
}

void printMemory(MemoryContext* mc){
    Block* block=mc->blockList;
    int b=0;
    printf("----------------------------------------------------------\n");
    printf("%7s%18s%7s%8s\n","Block","Pointer","Size","isFree");
    printf("----------------------------------------------------------\n");
    while (block)
    {
        ++b;
        void* ptr=block->data + H_size;
        while(ptr <= block->currptr){
            printf("%7d%18p%7d%8d\n",b,ptr,getSize(ptr),isFree(ptr));
            ptr= ptr + getSize(ptr)+H_size;
        }
        block=block->nextBlock;
        if(block)printf("\n");
    }
    printf("----------------------------------------------------------\n");
}

void printFree(MemoryContext* mc){
    void* ptr=mc->freePtr;
    if(!ptr){
        printf("Nothing Free!\n");return ;
    }
    printf("-------------------\n");
    printf("Free Memory List:\n");
    printf("-------------------\n");
    while(1){
        printf("%p \n",ptr);
        ptr = *((void**) ptr+1);
        // printf("next: %p\n",ptr);
        if(ptr==NULL)break;
    }
    printf("-------------------\n");
}

void user(MemoryContext* mc,int n){

    // printf("%d\n",getpid());

    mc=createMemoryContext();

    srand(time(NULL));
    void** ptrs = (void**)malloc(n * sizeof(void*));

    if(ptrs==NULL){
        printf("Unable to create Required size!\n");
        return ;
    }
     
    for(int i=0;i<n;i++){
        ptrs[i]=NULL;
    }

    int tempSize ;

    for(int i=0;i<n;i++){
        
        tempSize = rand() % tempMaxChunkSize + 1;

        ptrs[i] =memoryAlloc(mc,tempSize);

        if(ptrs[i]!=NULL){
            strncpy(ptrs[i],tempString,tempSize-1);
            // ptrs[i][tempSize-1]='\0';
        }
    }

    int ind;
    for(int i=0;i<n/10;i++){
        
        ind =rand()%n;
        if(ptrs[ind]!=NULL){
            memFree(ptrs[ind],mc);
            ptrs[ind]=NULL;
        }
    }

    for(int i=0;i<n/1000;i++){
        ind = rand()% n;
        if(ptrs[ind]!=NULL && !isFree(ptrs[ind])){
            tempSize = rand() % tempMaxChunkSize +1;
            void* temp =memoryRealloc(ptrs[ind],tempSize,mc);
            if(temp!=NULL){
                ptrs[ind]=temp;
            }
        }
    }
    
    // sleep(20);

    destroyMemoryContext(mc);
    mc=NULL;
}