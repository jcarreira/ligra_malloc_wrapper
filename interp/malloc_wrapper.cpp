#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <map>
#include <sys/mman.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <vector>

#define GB (1024*1024*1024L)
#define MAX_MEMORY 3*GB
#define NS_IN_SEC 1000000000L
#define MAX(a,b) ((a)>(b)?(a):(b))
#define BIG_MEM_SIZE (4*GB)
#define PAGE_SIZE (4*1024L)

int inside_malloc = 0;
int inside_free = 0;
int inside_calloc = 0;

//std::map<void*, int> addr_to_size;
std::vector<std::pair<void*, unsigned long long int> > addr_to_size;


unsigned long int total_allocated = 0;
unsigned long int max_allocated = 0;
unsigned long int total_elapsed_malloc = 0;
unsigned long int count_malloc = 0;

typedef void* (*malloc_type)(size_t);
malloc_type real_malloc;
typedef void (*free_type)(void*);
free_type real_free;
typedef void* (*realloc_type)(void*, int);
realloc_type real_realloc;

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
        exit(-1);
    }
}

void setup_handler()
{
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        exit(-1);
    }
}


static void mtrace_init(void)
{
    real_malloc = (malloc_type)1;
    real_free = (free_type)1;

    addr_to_size.reserve(100000);
    setup_handler();

    //real_malloc = (malloc_type)dlsym(RTLD_NEXT, "malloc");
    //if (NULL == real_malloc) {
    //    fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    //}

    //real_free = (free_type)dlsym(RTLD_NEXT, "free");
    //if (NULL == real_free) {
    //    fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    //}

    //big_mem = new char[BIG_MEM_SIZE];
    //big_mem[0] = 0;
}

//#define BASE_ADDRESS  0x800000000L
//void* cur_addr = (void*)BASE_ADDRESS;
        
void insert_addr_to_size(void *p, unsigned long int size_allocation) {
    //addr_to_size[p] = size_allocation;
    addr_to_size.push_back(std::make_pair(p, size_allocation));
    //addr_to_size[addr_to_size.size()] = std::make_pair(p, size_allocation);
}

void erase(void *p) {
    for (unsigned int i = 0; i < addr_to_size.size(); ++i) {
        if (addr_to_size[i].first == p)
            addr_to_size.erase(addr_to_size.begin() + i);
    }
}

bool exists(void *p) {
    for (unsigned int i = 0; i < addr_to_size.size(); ++i) {
        if (addr_to_size[i].first == p)
            return true;
    }
    return false;
}

unsigned long long int get_size(void *p) {
    for (unsigned int i = 0; i < addr_to_size.size(); ++i) {
        if (addr_to_size[i].first == p)
            return addr_to_size[i].second;
    }
    exit(-1);
}

void *malloc(size_t size)
{
    unsigned long long start = get_time();
    inside_malloc++;

    if(real_malloc==NULL) {
        mtrace_init();
    }

    void *p = NULL;
    fprintf(stderr, "malloc(%lu). count: %lu\n", size, addr_to_size.size());
    //p = real_malloc(size);

    unsigned long long size_allocation = size/PAGE_SIZE*PAGE_SIZE+PAGE_SIZE;

    p = mmap(NULL, size_allocation,
    //p = mmap((void*)cur_addr, size_allocation,
                PROT_EXEC|PROT_READ|PROT_WRITE, 
            MAP_PRIVATE|MAP_ANONYMOUS, 0, 0); 
            //MAP_FIXED | MAP_PRIVATE|MAP_ANONYMOUS, 0, 0); 
    //cur_addr += size_allocation;
    if (p == MAP_FAILED) {
        fprintf(stderr, "Error mmap. errno: %d\n", errno);
        exit(-1);
    }  
    fprintf(stderr, "malloc(%lu) = %p\n", size, p);


    //if (count_malloc >= 100 && count_malloc <= 500) {
    //    int ret = mprotect(p, MAX((size/PAGE_SIZE) * PAGE_SIZE, PAGE_SIZE), PROT_NONE);
    //    printf("int ret = mprotect(0x%X, %d, PROT_NONE)\n", p, MAX( (size/PAGE_SIZE) * PAGE_SIZE, PAGE_SIZE));
    //    printf("mprotect: %d errno: %d aligned: %d\n", ret, errno, (((unsigned long)p) & (PAGE_SIZE-1)) == 0);
    //}

    if (inside_free == 0 && inside_malloc == 1) {
        //fprintf(stderr, "addr_to_size[%p] = %d;\n", p, size_allocation);

        insert_addr_to_size(p, size_allocation);
        total_allocated += size_allocation;
        max_allocated = MAX(max_allocated, total_allocated);
    } else {
        fprintf(stderr, "not added inside_malloc: %d inside_free: %d\n", inside_malloc, inside_free);
    }

    unsigned long long elapsed = get_time() - start;
    total_elapsed_malloc += elapsed;
    count_malloc++;

    if (count_malloc % 100==0) {
        fprintf(stderr, "average elapsed malloc: %lu ns\n", total_elapsed_malloc/count_malloc);
        fprintf(stderr, "total = %luM max_allocated: %luM\n", total_allocated/1024/1024, max_allocated/1024/1024);
        fprintf(stderr, "count_malloc: %lu\n", count_malloc);
    }
    printf("time elapsed: %lld ns\n", elapsed);
    
    inside_malloc--;

    return p;
}

void *calloc(size_t nmemb, size_t size){

    inside_calloc++;

    if (inside_calloc > 1) {
        void* p = mmap(NULL, PAGE_SIZE, PROT_EXEC|PROT_READ|PROT_WRITE, 
                MAP_FIXED | MAP_PRIVATE|MAP_ANONYMOUS, 0, 0); 
        memset(p, 0, nmemb*size);
        return p;
    }

    if(real_malloc==NULL) {
        mtrace_init();
    }

    fprintf(stderr, "calloc\n");
    void* p = malloc(nmemb*size);
    if (p == 0) {
        fprintf(stderr, "error calloc\n");
        exit(-1);
    }
    memset(p, 0, nmemb*size);

    inside_calloc--;
    return p;
}

void *realloc(void *ptr, size_t size){
    fprintf(stderr, "realloc\n");

    //if (addr_to_size.find(ptr) == addr_to_size.end()) {

    //    if (NULL == real_malloc) {
    //        real_realloc = (realloc_type)dlsym(RTLD_NEXT, "realloc");
    //        if (NULL == real_malloc) {
    //            fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    //            exit(-1);
    //        }
    //    }

    //    fprintf(stderr, "realloc: addr not found\n");
    //    return real_realloc(ptr, size);
    //    //exit(-1);
    //}

    if (ptr == NULL && size == 0) {
        fprintf(stderr, "error realloc\n");
        exit(-1);
    }
    else if (ptr == NULL) {
        return malloc(size);
        //fprintf(stderr, "error realloc\n");
        //exit(-1);
    }
    else {
        void* p = malloc(size);
        memcpy(p, ptr, size);
        //free(ptr);
        return p;
    }
}

void free(void* addr)
{
    fprintf(stderr, "munmap(%p)\n", addr);

    if (addr == NULL)
        return;

    inside_free++;

    if(real_free==NULL) {
        mtrace_init();
    }

    //    fprintf(stderr, "free(0x%lu)\n", (unsigned long int)addr);
    //real_free(addr);

    if (!exists(addr)) {
    //if (addr_to_size.find(addr) == addr_to_size.end()) {
        fprintf(stderr, "free: addr %p not found\n", addr);
        inside_free--;
        return;
        //exit(-1);
    }

    int ret = munmap(addr, get_size(addr));
    //int ret = munmap(addr, addr_to_size[addr]);
    //fprintf(stderr, "munmap(%p, %d)\n", addr, addr_to_size[addr]);
    if (ret != 0) {
        fprintf(stderr, "Error munmap\n");
        exit(-1);
    }
    
    if (inside_malloc == 0 && inside_free == 1 && exists(addr)) {
    //if (inside_malloc == 0 && inside_free == 1 && addr_to_size.find(addr) != addr_to_size.end()) {
        total_allocated -= get_size(addr);//addr_to_size[addr];
        erase(addr);
        //addr_to_size.erase(addr);
    }

    inside_free--;
    fprintf(stderr, "munmap exit\n");
}


