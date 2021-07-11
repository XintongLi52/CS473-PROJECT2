/*
 * mm.c
 *
 * Name: Xintong Li, Maulik Gupta
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
#define DEBUG

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
#define WSIZE 8
#define DSIZE 16
#define CHUNKSIZE (1<<12)
#define MinBlockSize 32
static const uint64_t alloc_mask = 0x1;
static const uint64_t size_mask = ~(uint64_t)0xF;
typedef struct block
{
    /* Header contains size + allocation flag */
    uint64_t header;
    /*
     * We don't know how big the payload will be.  Declaring it as an
     * array of size 0 allows computing its starting address using
     * pointer notation.
     */
    char payload[0];
    /*
     * We can't declare the footer as part of the struct, since its starting
     * position is unknown
     */
} block_t;

static block_t *heap_listp = NULL;

static uint64_t PACK(size_t size, bool alloc);
static block_t *payload_to_header(void *bp);
static void write_header(block_t *block, size_t size, bool alloc);
static void write_footer(block_t *block, size_t size, bool alloc);
static block_t *find_next(block_t *block);
static bool get_alloc(block_t *block);
static bool extract_alloc(uint64_t word);
static size_t align(size_t x);
static uint64_t *find_prev_footer(block_t *block);
static size_t get_size(block_t *block);
static block_t *find_prev(block_t *block);
static void *coalesce(void *block);
static block_t *extend_heap(size_t size);
static block_t *find_fit(size_t asize);
static void place(block_t *block, size_t asize);
static bool aligned(const void* p);
static size_t max(size_t x, size_t y);
static void *header_to_payload(block_t *block);
static uint64_t get_payload_size(block_t *block);

static uint64_t PACK(size_t size, bool alloc)
{
    return (alloc ? (size | 1) : size);
}
static block_t *payload_to_header(void *bp)
{
    return (block_t *)(((char *)bp) - 8);
}
static void write_header(block_t *block, size_t size, bool alloc)
{
    block->header = PACK(size, alloc);
}
static void write_footer(block_t *block, size_t size, bool alloc)
{
    uint64_t *footp = (uint64_t *)((block->payload) + get_size(block) - DSIZE);
    *footp = PACK(size, alloc);
}
static block_t *find_next(block_t *block)
{
    block_t *block_next = (block_t *)(((char *)block) + get_size(block));
    return block_next;
}
/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}
static bool extract_alloc(uint64_t word)
{
    return (bool)(word & alloc_mask);
}
static uint64_t *find_prev_footer(block_t *block)
{
    // Compute previous footer position as one word before the header
    return (&(block->header)) - 1;
}
static bool get_alloc(block_t *block)
{
    return extract_alloc(block->header);
}
static size_t extract_size(uint64_t word)
{
    return (word & size_mask);
}
static size_t get_size(block_t *block)
{
    return extract_size(block->header);
}
static block_t *find_prev(block_t *block)
{
    uint64_t *footerp = find_prev_footer(block);
    size_t size = extract_size(*footerp);
    return (block_t *)((char *)block - size);
}
static size_t max(size_t x, size_t y)
{
    return (x > y) ? x : y;
}
static void *header_to_payload(block_t *block)
{
    return (void *)(block->payload);
}
static uint64_t get_payload_size(block_t *block)
{
    size_t asize = get_size(block);
    return asize - DSIZE;
}
static void *coalesce(void *block) {
    block_t *block_next = find_next(block);
    block_t *block_prev = find_prev(block);

    bool prev_alloc = extract_alloc(*(find_prev_footer(block)));
    bool next_alloc = get_alloc(block_next);
    size_t size = get_size(block);

    if (prev_alloc && next_alloc)              // Case 1
    {
        return block;
    }

    else if (prev_alloc && !next_alloc)        // Case 2
    {
        size += get_size(block_next);
        write_header(block, size, false);
        write_footer(block, size, false);
    }

    else if (!prev_alloc && next_alloc)        // Case 3
    {
        size += get_size(block_prev);
        write_header(block_prev, size, false);
        write_footer(block_prev, size, false);
        block = block_prev;
    }

    else                                        // Case 4
    {
        size += get_size(block_next) + get_size(block_prev);
        write_header(block_prev, size, false);
        write_footer(block_prev, size, false);

        block = block_prev;
    }
    return block;
}


static block_t *extend_heap(size_t size) {
    void *bp;

    // Allocate an even number of words to maintain alignment
    size = align(size);
    if ((bp = mem_sbrk(size)) == (void *)-1)
    {
        return NULL;
    }
    
    // Initialize free block header/footer 
    block_t *block = payload_to_header(bp);
    write_header(block, size, false);
    write_footer(block, size, false);
    // Create new epilogue header
    block_t *block_next = find_next(block);
    write_header(block_next, 0, true);

    // Coalesce in case the previous block was free
    return coalesce(block);
}



/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{
   uint64_t *start = (uint64_t *)(mem_sbrk(2*WSIZE));

    if (start == (void *)-1) 
    {
        return false;
    }
    start[0] = PACK(0, true); // Prologue footer
    start[1] = PACK(0, true); // Epilogue header
    // Heap starts with first block header (epilogue)
    heap_listp = (block_t *) &(start[1]);

    // Extend the empty heap with a free block of chunksize bytes
    if (extend_heap(CHUNKSIZE) == NULL)
    {
        return false;
    }
    return true;
}

static block_t *find_fit(size_t asize) {
     block_t *block;

    for (block = heap_listp; get_size(block) > 0;
                             block = find_next(block))
    {

        if (!(get_alloc(block)) && (asize <= get_size(block)))
        {
            return block;
        }
    }
    return NULL; // no fit found
}

static void place(block_t *block, size_t asize) {
    size_t csize = get_size(block);

    if ((csize - asize) >= MinBlockSize)
    {
        block_t *block_next;
        write_header(block, asize, true);
        write_footer(block, asize, true);

        block_next = find_next(block);
        write_header(block_next, csize-asize, false);
        write_footer(block_next, csize-asize, false);
    }

    else
    { 
        write_header(block, csize, true);
        write_footer(block, csize, true);
    }
}


/*
 * malloc
 */
void* malloc(size_t size)
{
    /* IMPLEMENT THIS */

    size_t asize;      // Adjusted block size
    size_t extendsize; // Amount to extend heap if no fit is found
    block_t *block;
    void *bp = NULL;

    if (heap_listp == NULL) // Initialize heap if it isn't initialized
    {
        mm_init();
    }

    if (size == 0) // Ignore spurious request
    {
        return bp;
    }

    // Adjust block size to include overhead and to meet alignment requirements
    asize = align(size) + DSIZE;

    // Search the free list for a fit
    block = find_fit(asize);

    // If no fit is found, request more memory, and then and place the block
    if (block == NULL)
    {  
        extendsize = max(asize, CHUNKSIZE);
        block = extend_heap(extendsize);
        if (block == NULL) // extend_heap returns an error
        {
            return bp;
        }

    }

    place(block, asize);
    bp = header_to_payload(block);
    return bp;
}


/*
 * free
 */
void free(void* bp)
{
     if (bp == NULL)
    {
        return;
    }

    block_t *block = payload_to_header(bp);
    size_t size = get_size(block);

    write_header(block, size, false);
    write_footer(block, size, false);

    coalesce(block);
}

/*
 * realloc
 */
void *realloc(void *ptr, size_t size)
{
    block_t *block = payload_to_header(ptr);
    size_t copysize;
    void *newptr;

    // If size == 0, then free block and return NULL
    if (size == 0)
    {
        free(ptr);
        return NULL;
    }

    // If ptr is NULL, then equivalent to malloc
    if (ptr == NULL)
    {
        return malloc(size);
    }

    // Otherwise, proceed with reallocation
    newptr = malloc(size);
    // If malloc fails, the original block is left untouched
    if (!newptr)
    {
        return NULL;
    }

    // Copy the old data
    copysize = get_payload_size(block); // gets size of old payload
    if(size < copysize)
    {
        copysize = size;
    }
    memcpy(newptr, ptr, copysize);

    // Free the old block
    free(ptr);

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
#endif /* DEBUG */
    return true;
}
