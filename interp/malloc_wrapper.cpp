#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <map>
#include <sys/mman.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define GB (1024*1024*1024L)
#define MAX_MEMORY 3*GB
#define NS_IN_SEC 1000000000L
#define MAX(a,b) ((a)>(b)?(a):(b))
#define BIG_MEM_SIZE (4*GB)
#define PAGE_SIZE (4*1024)

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

char* big_mem = 0;

unsigned long long int get_time()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec*NS_IN_SEC+ts.tv_nsec;
}

static void handler(int sig, siginfo_t *si, void *unused)
{
    printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);
    //addr = mmap((void*)0x8000000, 1024*4, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_FIXED | MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
    int ret = mprotect(si->si_addr, PAGE_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC);
    if (ret != 0) {
        printf("mprotect failed\n");
    }
}

void setup_handler()
{
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1)
        exit(-1);
}

static void mtrace_init(void)
{
    setup_handler();

    real_malloc = (malloc_type)dlsym(RTLD_NEXT, "malloc");
    if (NULL == real_malloc) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }

    real_free = (free_type)dlsym(RTLD_NEXT, "free");
    if (NULL == real_free) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }

    big_mem = new char[BIG_MEM_SIZE];
    big_mem[0] = 0;
}

void *malloc(size_t size)
{
    unsigned long long start = get_time();
    inside_malloc++;

    if(real_malloc==NULL) {
        mtrace_init();
    }

    void *p = NULL;
    fprintf(stderr, "malloc(%lu)\n", size);
    p = real_malloc(size);

    if (count_malloc >= 100 && count_malloc <= 500) {
        int ret = mprotect(p, MAX((size/PAGE_SIZE) * PAGE_SIZE, PAGE_SIZE), PROT_NONE);
        printf("int ret = mprotect(0x%X, %d, PROT_NONE)\n", p, MAX( (size/PAGE_SIZE) * PAGE_SIZE, PAGE_SIZE));
        printf("mprotect: %d errno: %d aligned: %d\n", ret, errno, (((unsigned long)p) & (sizeof(p)-1)) == 0);
    }

    if (inside_free == 0 && inside_malloc == 1) {
        addr_to_size[p] = size;
        total_allocated += size;
        max_allocated = MAX(max_allocated, total_allocated);
    }

    inside_malloc--;

    unsigned long long elapsed = get_time() - start;
    total_elapsed_malloc += elapsed;
    count_malloc++;

    if (count_malloc % 100==0) {
        fprintf(stderr, "average elapsed malloc: %lu ns\n", total_elapsed_malloc/count_malloc);
        fprintf(stderr, "total = %luM max_allocated: %luM\n", total_allocated/1024/1024, max_allocated/1024/1024);
        fprintf(stderr, "count_malloc: %lu\n", count_malloc);
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


