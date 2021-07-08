/*
 * mm.c
 *
 * Name: Xintong Li Maulik Gupta
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 * Also, read malloclab.pdf carefully and in its entirety before beginning.
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
// #define DEBUG

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

static void* HeapList;

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/* Static global variable and static functions*/
int WSIZE = 4;       //size for a single word
int DSIZ = 8;
int CHUNK = 1 << 8;     //Extend heap by this amount(bytes)

/* assign value to *p */
static void PUT ((unsigned char *) p, unsigned int value)
{
    *(unsigned int *)(p) = (val);

}

/* Read a word at address p */
static unsigned int GET(unsigned char * p)
{
    return (*(unsigned int *)(p));
}

static unsigned int GET_SIZE(unsigned char * p)
{
    return (GET(p) & ~0x7);
}

static unsigned int GET_ALLOC(unsigned char * p)
{
    return (GET(p) & 0x1);
}

/*pack a size and allocated bit into a word*/
static unsigned int PACK(unsigned int size, unsigned int alloc)
{
    unsigned int val = (size)|(alloc);
    return val;
}

/*Compute the address of block's pointer bp's header*/
static unsigned char * HDRP(unsigned char *bp)
{
    bp = bp - WSIZE;
    return bp;
}

/*Compute the address of block's pointer bp's footer*/
static unsigned char * FTRP(unsigned char *bp)
{
    bp = bp + GET_SIZE(HDRP(bp)) - DSIZE);
    return bp;
}

/*compute address of next blocks*/
static unsigned char * NEXT_BLKP(unsigned char *bp)
{
    bp = (char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE));
    return bp;

}

static unsigned char * PREV_BLKP(unsigned char *bp)
{
    bp = (char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE));
    return bp;

}

/*Given the number of words and extend the heap with a new free block*/
static void *extend_heap(size_t words)
2 {
3   char *bp;
4   size_t size;
5
6   /* Allocate an even number of words to maintain alignment */
7   size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
8   if ((long)(bp = mem_sbrk(size)) == -1)
9   return NULL;
10
11  /* Initialize free block header/footer and the epilogue header */
12  PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
13  PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
14  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
15
16  return bp;
    //return coalesce(bp);
18 }

static void *coalesce(void *bp) 
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {            /* Case 1 */
	return bp;
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2 */
	size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size,0));
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3 */
	size += GET_SIZE(HDRP(PREV_BLKP(bp)));
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	bp = PREV_BLKP(bp);
    }

    else {                                     /* Case 4 */
	size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
	    GET_SIZE(FTRP(NEXT_BLKP(bp)));
	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
	bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{
    /* IMPLEMENT THIS */
    if((HeapList = mem_sbrk(4*WSIZE)) == (void *)-1) return false; //return false if extending the heap is unsuccessful.
    PUT(HeapList,0);                                   //Alignment padding
    PUT(HeapList+(1*WSIZE), PACK(DSIZE,1));     //Prologue header
    PUT(HeapList+(2*WSIZE), PACK(DSIZE,1));     //Prologue footer
    PUT(HeapList+(3*WSIZE), PACK(0,1));             //Epilogue header
    HeapList += (2*;WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return false;
    return true;
}

/*
 * malloc
 */
void* malloc(size_t size)
{
    /* IMPLEMENT THIS */
    return NULL;
}

/*
 * free
 */
void free(void* ptr)
{
    /* IMPLEMENT THIS */
    size_t size = GET_SIZE(HDRP(bp));
4   PUT(HDRP(bp), PACK(size, 0));
6   PUT(FTRP(bp), PACK(size, 0));
7   //coalesce(bp);
    return;
}

/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{
    /* IMPLEMENT THIS */
    
    return NULL;
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
#endif /* DEBUG */
    return true;
}
