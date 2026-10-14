/* Omni-RISC APU - heap allocator (firmware track, F5).
 * Spec: firmware/include/omni/alloc.h.
 */
#include <stdint.h>
#include <omni/alloc.h>

#define MAGIC_USED 0xA110C8EDu
#define MAGIC_FREE 0xF4EEF4EEu
#define MIN_PAYLOAD 8u                          /* smallest useful split */

typedef struct {
    uint32_t size;                              /* total, including this header */
    uint32_t magic;
} blk_t;

#define HDR ((size_t)sizeof(blk_t))
#define HDR_ALIGNED ((HDR + OMNI_ALIGN - 1u) & ~(size_t)(OMNI_ALIGN - 1u))

static uint8_t arena[OMNI_HEAP_BYTES] __attribute__((aligned(OMNI_ALIGN)));
static unsigned errors;

static size_t align_up(size_t n) { return (n + OMNI_ALIGN - 1u) & ~(size_t)(OMNI_ALIGN - 1u); }

void heap_init(void)
{
    blk_t *b = (blk_t *)arena;
    b->size = OMNI_HEAP_BYTES;
    b->magic = MAGIC_FREE;
    errors = 0;
}

static int is_free(const blk_t *b) { return b->magic == MAGIC_FREE; }

/* Merge every pair of adjacent free blocks. Walking forward once is enough:
 * a merge can only extend the current block, so the next comparison is against
 * the (possibly bigger) current one. */
static void coalesce(void)
{
    blk_t *b = (blk_t *)arena;
    while ((size_t)((uint8_t *)b - arena) < OMNI_HEAP_BYTES) {
        blk_t *next = (blk_t *)((uint8_t *)b + b->size);
        if ((size_t)((uint8_t *)next - arena) >= OMNI_HEAP_BYTES) break;
        if (is_free(b) && is_free(next)) {
            b->size += next->size;              /* keep the same header */
            continue;                           /* try to swallow the next one too */
        }
        b = next;
    }
}

void *malloc(size_t n)
{
    blk_t *b;
    size_t need;

    if (n == 0) return 0;
    need = align_up(HDR_ALIGNED + n);
    if (need > OMNI_HEAP_BYTES) return 0;

    for (b = (blk_t *)arena; ; b = (blk_t *)((uint8_t *)b + b->size)) {
        if ((size_t)((uint8_t *)b - arena) >= OMNI_HEAP_BYTES) return 0;
        if (is_free(b) && b->size >= need) {
            /* split when the leftover can hold a header plus the minimum payload */
            if (b->size - need >= HDR_ALIGNED + MIN_PAYLOAD) {
                blk_t *rest = (blk_t *)((uint8_t *)b + need);
                rest->size = b->size - need;
                rest->magic = MAGIC_FREE;
                b->size = need;
            }
            b->magic = MAGIC_USED;
            return (uint8_t *)b + HDR_ALIGNED;
        }
    }
}

/* Find the block whose payload is p, validating as we go. Returns 0 for a
 * pointer that is not the start of a live block. */
static blk_t *find(void *p)
{
    blk_t *b;
    if (p == 0) return 0;
    for (b = (blk_t *)arena; (size_t)((uint8_t *)b - arena) < OMNI_HEAP_BYTES;
         b = (blk_t *)((uint8_t *)b + b->size)) {
        if ((uint8_t *)b + HDR_ALIGNED == (uint8_t *)p) {
            if (b->magic != MAGIC_USED) return 0;       /* double free */
            return b;
        }
    }
    return 0;
}

void free(void *p)
{
    blk_t *b = find(p);
    if (b == 0) {
        if (p) errors++;                                /* double free / wild pointer */
        return;
    }
    b->magic = MAGIC_FREE;
    coalesce();
}

void *calloc(size_t n, size_t size)
{
    size_t total;
    uint8_t *q;

    if (n == 0 || size == 0) return 0;
    if (n > OMNI_HEAP_BYTES || size > OMNI_HEAP_BYTES) return 0;
    total = n * size;
    if (total / n != size) return 0;                    /* multiply overflowed */
    q = (uint8_t *)malloc(total);
    if (q == 0) return 0;
    for (size_t i = 0; i < total; i++) q[i] = 0;
    return q;
}

void *realloc(void *p, size_t n)
{
    blk_t *b;
    size_t need, old;

    if (p == 0) return malloc(n);
    if (n == 0) { free(p); return 0; }

    b = find(p);
    if (b == 0) { errors++; return 0; }

    need = align_up(HDR_ALIGNED + n);
    old = b->size;
    if (need <= old) {
        if (old - need >= HDR_ALIGNED + MIN_PAYLOAD) {
            blk_t *rest = (blk_t *)((uint8_t *)b + need);
            rest->size = old - need;
            rest->magic = MAGIC_FREE;
            b->size = need;
            coalesce();
        }
        return p;                                       /* shrank in place */
    }

    /* Grow: try to absorb a free neighbour, otherwise move. */
    {
        blk_t *next = (blk_t *)((uint8_t *)b + b->size);
        if ((size_t)((uint8_t *)next - arena) < OMNI_HEAP_BYTES && is_free(next)
            && old + next->size >= need) {
            b->size = old + next->size;
            if (b->size - need >= HDR_ALIGNED + MIN_PAYLOAD) {
                blk_t *rest = (blk_t *)((uint8_t *)b + need);
                rest->size = b->size - need;
                rest->magic = MAGIC_FREE;
                b->size = need;
            }
            return p;
        }
    }
    {
        uint8_t *q = (uint8_t *)malloc(n);
        size_t   copy = old - HDR_ALIGNED;
        if (q == 0) return 0;
        if (copy > n) copy = n;
        for (size_t i = 0; i < copy; i++) q[i] = ((uint8_t *)p)[i];
        free(p);
        return q;
    }
}

size_t heap_free_bytes(void)
{
    size_t total = 0;
    blk_t *b;
    for (b = (blk_t *)arena; (size_t)((uint8_t *)b - arena) < OMNI_HEAP_BYTES;
         b = (blk_t *)((uint8_t *)b + b->size))
        if (is_free(b) && b->size > HDR_ALIGNED) total += b->size - HDR_ALIGNED;
    return total;
}

size_t heap_largest_free(void)
{
    size_t best = 0;
    blk_t *b;
    for (b = (blk_t *)arena; (size_t)((uint8_t *)b - arena) < OMNI_HEAP_BYTES;
         b = (blk_t *)((uint8_t *)b + b->size))
        if (is_free(b) && b->size > HDR_ALIGNED && b->size - HDR_ALIGNED > best)
            best = b->size - HDR_ALIGNED;
    return best;
}

unsigned heap_blocks(void)
{
    unsigned n = 0;
    blk_t *b;
    for (b = (blk_t *)arena; (size_t)((uint8_t *)b - arena) < OMNI_HEAP_BYTES;
         b = (blk_t *)((uint8_t *)b + b->size))
        n++;
    return n;
}

unsigned heap_errors(void) { return errors; }
void heap_reset_errors(void) { errors = 0; }

int heap_check(void)
{
    blk_t *b;
    size_t off = 0;
    int prev_free = 0;

    for (b = (blk_t *)arena; off < OMNI_HEAP_BYTES; b = (blk_t *)((uint8_t *)b + b->size)) {
        if (b->size < HDR_ALIGNED || (b->size % OMNI_ALIGN) != 0) return 0;
        if (b->magic != MAGIC_FREE && b->magic != MAGIC_USED) return 0;
        if (is_free(b) && prev_free) return 0;          /* uncoalesced neighbours */
        prev_free = is_free(b);
        off += b->size;
    }
    return off == OMNI_HEAP_BYTES;                      /* the walk must end exactly */
}
