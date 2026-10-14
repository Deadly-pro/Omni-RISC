/* Omni-RISC APU - heap allocator (firmware track, F5).
 *
 * An arena with boundary tags: every block carries a header (size, free flag,
 * magic) and the allocator walks them linearly. That is enough for first-fit,
 * split-on-allocate, coalesce-on-free and - more importantly - for an invariant
 * checker you can run after every operation instead of reasoning about
 * fragmentation in your head.
 *
 * Threading: none. Single core, single context (same assumption as F3's receive
 * path), so no locking, and none pretended.
 */
#ifndef OMNI_ALLOC_H
#define OMNI_ALLOC_H

#include <stddef.h>

#define OMNI_HEAP_BYTES 4096u       /* the whole arena */
#define OMNI_ALIGN      8u          /* every returned pointer is 8-byte aligned */

void   heap_init(void);

void  *malloc(size_t n);
void   free(void *p);
void  *calloc(size_t n, size_t size);
void  *realloc(void *p, size_t n);

/* Statistics, reported by the gate so fragmentation is visible rather than felt. */
size_t   heap_free_bytes(void);     /* usable payload bytes currently free */
size_t   heap_largest_free(void);
unsigned heap_blocks(void);         /* blocks in the arena (free + used) */
unsigned heap_errors(void);         /* double frees / wild pointers caught */
void     heap_reset_errors(void);

/* Walk the arena and verify every invariant: sizes aligned and within bounds,
 * the walk ending exactly at the arena end, no two adjacent free blocks, and
 * every block either free or carrying a valid magic. Returns 1 when clean. */
int      heap_check(void);

#endif /* OMNI_ALLOC_H */
