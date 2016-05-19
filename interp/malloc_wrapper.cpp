#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <map>
#include <sys/mman.h>
#include <signal.h>
#include <time.h>

#define GB (1024*1024*1024)
#define MAX_MEMORY 3*GB
#define NS_IN_SEC 1000000000L
#define MAX(a,b) ((a)>(b)?(a):(b))

int inside_malloc = 0;
int inside_free = 0;
std::map<void*, int> addr_to_size;
unsigned long int total_allocated = 0;
unsigned long int max_allocated = 0;
unsigned long int total_elapsed_malloc = 0;
unsigned long int count_malloc = 0;

typedef void* (*malloc_type)(size_t);
malloc_type real_malloc;

typedef void (*free_type)(void*);
free_type real_free;

unsigned long long int get_time()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec*NS_IN_SEC+ts.tv_nsec;
}

static void mtrace_init(void)
{
    real_malloc = (malloc_type)dlsym(RTLD_NEXT, "malloc");
    if (NULL == real_malloc) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }

    real_free = (free_type)dlsym(RTLD_NEXT, "free");
    if (NULL == real_free) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }
}

void *malloc(size_t size)
{
    unsigned long long start = get_time();
    inside_malloc++;

    if(real_malloc==NULL) {
        mtrace_init();
    }

    void *p = NULL;
//    fprintf(stderr, "malloc(%lu) = ", size);
    p = real_malloc(size);

    if (inside_free == 0 && inside_malloc == 1) {
        addr_to_size[p] = size;
        total_allocated += size;
        max_allocated = MAX(max_allocated, total_allocated);
    }

    inside_malloc--;
    
    unsigned long long elapsed = get_time() - start;
    total_elapsed_malloc += elapsed;
    count_malloc++;

    if (count_malloc % 50==0) {
        printf("average elapsed malloc: %lu ns\n", total_elapsed_malloc/count_malloc);
        fprintf(stderr, "total = %luM max_allocated: %luM\n", total_allocated/1024/1024, max_allocated/1024/1024);
    }
    //printf("time elapsed: %lld ns\n", elapsed);
    return p;
}

void free(void* addr)
{
    inside_free++;

    if (inside_malloc == 0 && inside_free == 1 && addr_to_size.find(addr) != addr_to_size.end()) {
        total_allocated -= addr_to_size[addr];
        addr_to_size.erase(addr);
    }


    if(real_free==NULL) {
        mtrace_init();
    }

//    fprintf(stderr, "free(0x%lu)\n", (unsigned long int)addr);
    real_free(addr);

    inside_free--;
}


