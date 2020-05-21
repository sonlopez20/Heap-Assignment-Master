/*
*
*
*
*
*/

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>

#define ALIGN4(s)         (((((s) - 1) >> 2) << 2) + 4)
#define BLOCK_DATA(b)      ((b) + 1)
#define BLOCK_HEADER(ptr)   ((struct _block *)(ptr) - 1)


static int atexit_registered = 0;
static int num_mallocs       = 0;
static int num_frees         = 0;
static int num_reuses        = 0;
static int num_grows         = 0;
static int num_splits        = 0;
static int num_coalesces     = 0;
static int num_blocks        = 0;
static int num_requested     = 0;
static int max_heap          = 0;

/*
 *  \brief printStatistics
 *
 *  \param none
 *
 *  Prints the heap statistics upon process exit. Registered
 *  via atexit()
 *
 *  \return none
 */
void printStatistics( void ){

  printf("\nheap management statistics\n");
  printf("mallocs:\t%d\n", num_mallocs );
  printf("frees:\t\t%d\n", num_frees );
  printf("reuses:\t\t%d\n", num_reuses );
  printf("grows:\t\t%d\n", num_grows );
  printf("splits:\t\t%d\n", num_splits );
  printf("coalesces:\t%d\n", num_coalesces );
  printf("blocks:\t\t%d\n", num_blocks );
  printf("requested:\t%d\n", num_requested );
  printf("max heap:\t%d\n", max_heap );
}

struct _block 
{
   size_t  size;         /* Size of the allocated _block of memory in bytes */
   struct _block *next;  /* Pointer to the next _block of allcated memory   */
   bool   free;          /* Is this _block free? */
   char   padding[3];
};

struct _block *freeList = NULL; /* Free list to track the _blocks available */
struct _block *nextStart = NULL; /* Track starting point block for next fit */

/*
 * \brief findFreeBlock
 *
 * \param last pointer to the linked list of free _blocks
 * \param size size of the _block needed in bytes 
 *
 * \return a _block that fits the request or NULL if no free _block matches
 *
 */
struct _block *findFreeBlock(struct _block **last, size_t size){

  struct _block *curr = freeList;

#if defined FIT && FIT == 0
   /* First fit */
   
   while(curr && !(curr->free && curr->size >= size)) 
   {
     *last = curr;
     curr = curr->next;
   }

#endif

#if defined BEST && BEST == 0
   /* Best fit */

   struct _block *candidate = NULL;
   struct _block *temp = NULL;
   
   while(curr)
   {
     if(curr->free == true && curr->size >= size)
     {
       if(candidate != NULL) // If potential best fit is not null
	 {
	   if(candidate->size > curr->size) // If there is a current smaller block size
	   {
	     candidate = curr; // Candidate block is now the new best fit
	   }
	 }
	 else // If candidate block is null
	 {
	   temp = curr;
	   candidate = temp;
	 }
     }
    
     curr = curr->next;
  }
  
  if(candidate != NULL && curr == NULL) // If current block is empty and candidate block is not,
  {                                     // update current block
    curr = candidate;
  }

#endif

#if defined WORST && WORST == 0
   /* Worst fit */
   
   struct _block *candidate = NULL;
   struct _block *temp = NULL;

   while(curr)
   {
     if(curr->free == true && curr->size >= size)
     {
       if(candidate != NULL) // If potential best fit is not null
       {
         if(candidate->size < curr->size) // If there is a current larger block size
         {
           candidate = curr; // Candidate block is now the new worst fit
         }
       }
       else
       {
         temp = curr;
	   candidate = temp;
	 }
     }

     curr = curr->next;
   }
  
   if(candidate != NULL && curr == NULL) // If current block is empty and candidate block is not,
   {                                     // update current block
     curr = candidate;
   }

#endif

#if defined NEXT && NEXT == 0
   /* Next fit */
 
   curr = nextStart; // Update current starting point to global starting point
   
   if(nextStart != NULL)
   {
     curr = nextStart;
   }

   while(curr && !(curr->free && curr->size >= size))
   {
     *last = curr;
     curr  = curr->next;
   }
   
   if(curr) // If current block is not empty, track where to continue looking
   {
     nextStart = curr->next;
   }
  
#endif

   return curr; // Return current block for further processing
}

/*
 * \brief growheap
 *
 * Given a requested size of memory, use sbrk() to dynamically 
 * increase the data segment of the calling process.  Updates
 * the free list with the newly allocated memory.
 *
 * \param last tail of the free _block list
 * \param size size in bytes to request from the OS
 *
 * \return returns the newly allocated _block of NULL if failed
 */
struct _block *growHeap(struct _block *last, size_t size){

   /* Request more space from OS */
   struct _block *curr = (struct _block *)sbrk(0);
   struct _block *prev = (struct _block *)sbrk(sizeof(struct _block) + size);

   assert(curr == prev);

   /* OS allocation failed */
   if(curr == (struct _block *)-1) 
   {
     return NULL;
   }

   /* Update freeList if not set */
   if(freeList == NULL) 
   {
     freeList = curr;
   }

   /* Attach new _block to prev _block */
   if(last) 
   {
     last->next = curr;
   }

   /* Update _block metadata */
   curr->size = size;
   curr->next = NULL;
   curr->free = false;
   max_heap += size; // Update running total of heap size
   num_grows++; // Update number of times a new block is requested
   return curr;
}

/*
 * \brief malloc
 *
 * finds a free _block of heap memory for the calling process.
 * if there is no free _block that satisfies the request then grows the 
 * heap and returns a new _block
 *
 * \param size size of the requested memory in bytes
 *
 * \return returns the requested memory allocation to the calling process 
 * or NULL if failed
 */
void *malloc(size_t size){

   num_requested += size;

   if(atexit_registered == 0)
   {
     atexit_registered = 1;
     atexit( printStatistics );
   }

   /* Align to multiple of 4 */
   size = ALIGN4(size);

   /* Handle 0 size */
   if(size == 0) 
   {
     return NULL;
   }

   /* Look for free _block */
   struct _block *last = freeList;
   struct _block *next = findFreeBlock(&last, size);

   if(next != NULL) 
   {
     num_reuses++; // Update number of times a block is reused
   }
   
   // Attempt to split the free block 
   if(next != NULL && next->size > (sizeof(struct _block) + size))
   {
     void *pointer = BLOCK_DATA(next); // Get address of where the block data starts		
     pointer += sizeof(char)*size; // Move pointer to end of block data
		
     struct _block *newBlock = BLOCK_HEADER(pointer); // Start of new block		
     newBlock->size = next->size - sizeof(struct _block) - size; // Fix size of new block
                                                                     
     newBlock->free = true; // Set new block to free                                      
     newBlock->next = next->next; // Point new block to next block                                 
                                                                    
     next->next = newBlock; // Next block points to new block                                      
     next->size = size; // Fix block size to requested size                                     
                                                                     
                                                                     
     num_splits++; // Update number of times a block was split 
     num_blocks++; // Update number of blocks in the free list
   }	   
   
   if(next == NULL) // No new block, try to grow the heap
   {
     next = growHeap(last, size); // Grow the heap
	  
     if(next != NULL)
     {
       num_blocks++; // Update number of blocks in the free list
     }
   }
   
   if(next == NULL) // No free block AND cannot grow heap
   {
     return NULL;
   }
   
   next->free = false; // Set block to not free
   num_mallocs++; // Update number of times malloc succeeded
   
   return BLOCK_DATA(next); // Return address of next block
}

/*
 * \brief calloc
 *
 * finds a free _block of heap memory for the calling process.
 * if there is no free _block that satisfies the request then grows the 
 * heap and returns a new _block
 *
 * \param nmemb number of blocks to allocate
 * \param size size of the requested memory in bytes
 *
 * \return returns the requested memory allocation to the calling process 
 * or NULL if failed
 */
void *calloc(size_t nmemb, size_t size){
	
   size_t trueSize = nmemb*size; // Actual block size to allocate
	
   num_requested += trueSize; // Update running total of blocks requested
	
   if(atexit_registered == 0)
   {
     atexit_registered = 1;
     atexit(printStatistics);
   }
	
   /* Align to multiple of 4 */
   trueSize = ALIGN4(trueSize);
	
   /* Handle 0 size */
   if(trueSize == 0)
   {
     return NULL;
   }
	
   /* Look for a free block */
   struct _block *last = freeList;
   struct _block *next = findFreeBlock(&last, trueSize);

   if(next != NULL)
   {
     memset(BLOCK_DATA(next), 0, trueSize); // Zero out allocated memory 
     num_reuses++; // Update total number of block reuses
   }
	
   // Attempt to split the free block
   // Check if size of next block is greater than absolute size
   if(next != NULL && next->size > (sizeof(struct _block) + trueSize)) 
   {                                                                   
     void *pointer = BLOCK_DATA(next); // Get address of where data begins
     pointer += sizeof(char)*trueSize; // Move the pointer to the end of the data
		
     struct _block *newBlock = BLOCK_HEADER(pointer); // Starting address of new block
		
     newBlock->size = next->size - sizeof(struct _block) - trueSize; // Fix size of new block
		
     newBlock->free = true; // Set block to free
     newBlock->next = next->next; // Point the new block to the next block
		
     next->next = newBlock; // Next block points to the new block
     next->size = trueSize; // Fix the next block size to be the amount requested
		
     num_splits++; // Update number of splits made
     num_blocks++; // Update number of blocks in the free list
   }
    
   // No free block, grow the heap
   if(next == NULL)
   {
     next = growHeap(last, trueSize);
  
     if (next != NULL)
     {
       memset(BLOCK_DATA(next), 0, trueSize); // Zero out allocated memory 
       num_blocks++; // Update number of blocks in the free list
     }
   }

   // No free block AND cannot grow heap 
   if(next == NULL)
   {
    return NULL;
   }

   // Set the block to not free
   next->free = false;

   // Return address of the next block
   return BLOCK_DATA(next);	
}

/*
 * \brief free
 *
 * frees the memory _block pointed to by pointer. if the _block is adjacent
 * to another _block then coalesces (combines) them
 *
 * \param ptr the heap memory to free
 *
 * \return none
 */
void free(void *ptr){

  if(ptr == NULL) 
  {
    return;
  }

  struct _block *curr = BLOCK_HEADER(ptr);
  assert(curr->free == 0);
  curr->free = true; // Set the block to free
  num_frees++; // Update number of times free succeeds
   
  struct _block *tempOne = freeList;
   
  // Attempt to coalesce blocks
  while(tempOne != NULL)
  {
    if(tempOne->next != NULL && tempOne->free == true) // If free block is found that is not 
    {                                                  // the previous block
      if(tempOne->next->free == true) // If the next block is free
 	{
        struct _block *tempTwo = tempOne->next; // Next block to coalesce
	  tempOne->next = tempTwo->next; // Coalesce
        tempOne->size += tempTwo->size; // Size of newly coalesced block

        num_coalesces++; // Update number of times a coalesce is made
        num_blocks--; // Update number of blocks in the free list
			   
        continue;			   
      }
    }
	   
     tempOne = tempOne->next;
   }
}

/* vim: set expandtab sts=3 sw=3 ts=6 ft=cpp: --------------------------------*/
