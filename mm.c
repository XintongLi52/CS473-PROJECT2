/*
 * mm.c
 *
 * Name: Xintong Li, Maulik Gupta
 * 
 * Note: This is to attest to the below mentioned code's similarity 
 * to the example in Chapter 9 of the CSAPP, given its influence on 
 * the development of the program.
 * 
 * This memory allocator project uses implicit list and first-fit policy.
 * The alignment is 16.
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
//#define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#endif /* DEBUG */

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* DRIVER */

/* What is the correct alignment? */
#define ALIGNMENT 16

/* Constants and Macros */
#define WSIZE 8                 /*size for word and header/footer*/
#define DSIZE 16                /*size for double word*/
#define CHUNKSIZE (1<<12)


static unsigned long MAX(size_t x, size_t y) {
    if (x > y) return x;
    return y;
}

/*The belowmentioned helper function is mentioned on CSAPP, p830*/
/* Pack the size and allocated bit into a word.*/
static unsigned long PACK(size_t size, size_t alloc) {return (size | alloc);}
static unsigned long GET(void* p) {return (*(unsigned long *)(p));}

static unsigned long PUT(void* p, unsigned long val) {
    (*(unsigned long *) p) = val;
    return (*(unsigned long *)(p));
}

static unsigned long GET_SIZE(void* p) {return GET(p) & ~0x7;}
static unsigned long GET_ALLOC(void* p) {return GET(p) & 0x1;}

static char * HDRP(char* bp) {return (char *)(bp) - WSIZE;}
static char * FTRP(char* bp) {return ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE);}


static char * NEXT_BLKP(char* bp) {return ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)));}
static char * PREV_BLKP(char* bp) {return ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)));}


static char *heap_listp = 0;

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/*If the previous and next block is unallocated, it coalesces them. Otherwise, the block is not modified.*/
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));        
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) return bp;
    
    else if (prev_alloc && !next_alloc) {                   //case 1
        size = size + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {                   //case 2
        size = size + GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {                                                  //case 3
        size = size + GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*Extends the heap with words number of bytes. Returns by calling coalesce*/
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL;

    PUT (HDRP(bp), PACK(size, 0));
    PUT (FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}



/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) return false;

    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));  /*set allocated bit of prologue header to 1*/
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));  /*set allocated bit of prologue footer to 1*/
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));

    heap_listp = heap_listp + (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return false;

    return true;
}

/*looks for an unallocated block with the given size by using first-fit policy*/
static void *find_fit(size_t asize) {
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) return bp;
    }
    return NULL;
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));
    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}


/*
 * allocates a block with given size.
 */
void* malloc(size_t size)
{
    /* IMPLEMENT THIS */

    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;      

    /* $end mmmalloc */
    if (heap_listp == 0){
        mm_init();
    }
    /* $begin mmmalloc */
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)                                          //line:vm:mm:sizeadjust1
        asize = 2*DSIZE;                                        //line:vm:mm:sizeadjust2
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); //line:vm:mm:sizeadjust3
        //asize = align(size);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {  //line:vm:mm:findfitcall
        place(bp, asize);                  //line:vm:mm:findfitplace
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);                 //line:vm:mm:growheap1
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)  
        return NULL;                                  //line:vm:mm:growheap2
    place(bp, asize);                                 //line:vm:mm:growheap3
    return bp;
    //mm_checkheap(10);
}


/*
 * frees the block pointed to by bp
 */
void free(void* bp)
{
    /* IMPLEMENT THIS */
    if (bp == 0) return;
    if (heap_listp == 0){
        mm_init();
    }

    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);

}

/*
 * The realloc function returns a pointer to an allocated region of at least size bytes
 */
void* realloc(void* oldptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        mm_free(oldptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(oldptr == NULL) {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(oldptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);

    /* Free the old block. */
    mm_free(oldptr);

    return newptr;
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 */
bool mm_checkheap(int lineno)
{
#ifdef DEBUG
    /* Write code to check heap invariants here */
    /* IMPLEMENT THIS */
    dbg_printf("mm_checkheap\n");
    char *bp = heap_listp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if ((size_t)bp % ALIGNMENT != 0)
        {
            dbg_printf("Block is not aligned\n");
        }
        size_t size = GET_SIZE(HDRP(bp));
        if(size % ALIGNMENT != 0)
        {
            dbg_printf("Block size is not aligned\n");
        }
        if(!GET_ALLOC(HDRP(NEXT_BLKP(bp))) && !GET_ALLOC(HDRP(bp)))
        {
            dbg_printf("Contiguous free blocks\n");
        }
    }
        if (!(GET_ALLOC(HDRP(bp))))
         {
            printf("Bad epilogue header\n");
         }
#endif /* DEBUG */
    return true;
}

/*This project uses implicit free list with boundary tags. It stores the size and 
allocated word at both header and footer of the block. When freeing a block, 
its previous and successive block is checked to make sure that we coalesce the 
potential previous or successive free block. It perform a first-fit search, which 
search for the first unallocated block in the free list. The checkheap checks whether 
the block address is aligned, block size is aligned, or any contiguous free blocks existed.
In place function, the block will be splitted only if its remainder size is larger than 32. 
*/
