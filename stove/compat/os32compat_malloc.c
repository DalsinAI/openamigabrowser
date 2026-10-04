/* OpenBrowser (DalsinAI/openamigabrowser). Copyright (c) 2026 Dalsin Limited. MIT licence, see LICENSE. */
/* aligned_alloc() for libnix, which has none, so that free() still works.
 *
 * Linked with -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free
 * (and aligned_alloc/memalign/posix_memalign defined here), every block
 * carries a small header in front of the pointer handed out:
 *   [ ... padding ... ][ size ][ raw pointer ][ magic ][ user data ... ]
 * so free() and realloc() can find what libnix's malloc() really returned,
 * whatever the alignment asked for. */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void* __real_malloc(size_t);
void __real_free(void*);

#define OS32_MAGIC 0xA11C0DEDu
#define OS32_HEADER 12u

static void* os32_alloc(size_t alignment, size_t size)
{
    uintptr_t raw, user;
    size_t total;
    if (alignment < 8)
        alignment = 8;
    if (alignment & (alignment - 1))
        return NULL;
    if (size > (size_t)-1 - alignment - OS32_HEADER)
        return NULL;
    total = size + alignment + OS32_HEADER;
    raw = (uintptr_t)__real_malloc(total);
    if (!raw)
        return NULL;
    user = (raw + OS32_HEADER + alignment - 1) & ~(uintptr_t)(alignment - 1);
    ((uint32_t*)user)[-1] = OS32_MAGIC;
    ((uintptr_t*)user)[-2] = raw;
    ((size_t*)user)[-3] = size;
    return (void*)user;
}

void* __wrap_malloc(size_t size) { return os32_alloc(8, size); }

void __wrap_free(void* p)
{
    if (!p)
        return;
    if (((uint32_t*)p)[-1] != OS32_MAGIC)
        return; /* not ours: leak rather than corrupt libnix's heap */
    ((uint32_t*)p)[-1] = 0;
    __real_free((void*)((uintptr_t*)p)[-2]);
}

void* __wrap_calloc(size_t n, size_t size)
{
    void* p;
    if (size && n > (size_t)-1 / size)
        return NULL;
    p = os32_alloc(8, n * size);
    if (p)
        memset(p, 0, n * size);
    return p;
}

void* __wrap_realloc(void* old, size_t size)
{
    void* p;
    size_t oldSize;
    if (!old)
        return os32_alloc(8, size);
    if (!size) {
        __wrap_free(old);
        return NULL;
    }
    oldSize = ((size_t*)old)[-3];
    p = os32_alloc(8, size);
    if (!p)
        return NULL;
    memcpy(p, old, oldSize < size ? oldSize : size);
    __wrap_free(old);
    return p;
}

void* aligned_alloc(size_t alignment, size_t size) { return os32_alloc(alignment, size); }
void* memalign(size_t alignment, size_t size) { return os32_alloc(alignment, size); }

int posix_memalign(void** out, size_t alignment, size_t size)
{
    void* p;
    if (alignment < sizeof(void*) || (alignment & (alignment - 1)))
        return EINVAL;
    p = os32_alloc(alignment, size);
    if (!p)
        return ENOMEM;
    *out = p;
    return 0;
}

size_t os32_malloc_usable_size(void* p) { return p ? ((size_t*)p)[-3] : 0; }
