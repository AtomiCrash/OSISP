#define _GNU_SOURCE
#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <inttypes.h>
#include <limits.h>

size_t g_memsize;           
int g_block_count;        
int g_thread_count;       
int g_block_size;     
int g_next_block = 0;      

pthread_mutex_t g_block_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t g_barrier;

index_record *g_records = NULL;  
uint64_t g_total_records = 0;  

typedef struct {
    off_t offset;
    uint64_t nrecords;
} run_boundary;


int cmp_index(const void *a, const void *b)
{
    const index_record *ia = a;
    const index_record *ib = b;
    if (ia->time_mark < ib->time_mark)
        return -1;
    else if (ia->time_mark > ib->time_mark)
        return 1;
    return 0;
}

void merge_blocks(index_record *left, index_record *right, int nleft, int nright)
{
    int total = nleft + nright;
    index_record *tmp = malloc(total * sizeof(index_record));
    if (!tmp) {
        perror("malloc merge");
        exit(EXIT_FAILURE);
    }
    int i = 0, j = 0, k = 0;
    while (i < nleft && j < nright) {
        if (cmp_index(&left[i], &right[j]) <= 0)
            tmp[k++] = left[i++];
        else
            tmp[k++] = right[j++];
    }
    while (i < nleft)
        tmp[k++] = left[i++];
    while (j < nright)
        tmp[k++] = right[j++];
    memcpy(left, tmp, total * sizeof(index_record));
    free(tmp);
}

void *thread_sort(void *arg)
{
    (void)arg; 
    pthread_barrier_wait(&g_barrier);

    while (1) {
        int block_index = -1;
        pthread_mutex_lock(&g_block_mutex);
        if (g_next_block < g_block_count) {
            block_index = g_next_block;
            g_next_block++;
        }
        pthread_mutex_unlock(&g_block_mutex);
        if (block_index == -1)
            break;
        index_record *block_start = g_records + block_index * g_block_size;
        qsort(block_start, g_block_size, sizeof(index_record), cmp_index);
    }

    pthread_barrier_wait(&g_barrier);
    return NULL;
}

void *thread_merge(void *arg)
{
    (void)arg;
    while (1) {
        int merge_index = -1;
        pthread_mutex_lock(&g_block_mutex);
        if (g_next_block + 1 < g_block_count) {
            merge_index = g_next_block;
            g_next_block += 2;
        }
        pthread_mutex_unlock(&g_block_mutex);
        if (merge_index == -1)
            break;        
        index_record *left = g_records + merge_index * g_block_size;
        index_record *right = g_records + (merge_index + 1) * g_block_size;
        merge_blocks(left, right, g_block_size, g_block_size);
    }
    pthread_barrier_wait(&g_barrier);
    return NULL;
}

int process_portion(const char *filename, off_t current_offset, size_t portion_bytes, uint64_t *portion_records_out)
{
    int fd = open(filename, O_RDWR);
    if (fd < 0) {
        perror("open (process_portion)");
        return -1;
    }
    int pagesize = getpagesize();
    off_t map_offset = current_offset - (current_offset % pagesize);
    size_t map_length = portion_bytes + (current_offset - map_offset);

    void *mapped_region = mmap(NULL, map_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_offset);
    if (mapped_region == MAP_FAILED) {
        perror("mmap (process_portion)");
        close(fd);
        return -1;
    }
    close(fd);

    off_t start_offset; 
    uint64_t recs_in_portion;
    if (current_offset == 0) {
        start_offset = sizeof(uint64_t) + (current_offset - map_offset);
        if (portion_bytes < sizeof(uint64_t)) {
            fprintf(stderr, "Portion too small to contain header\n");
            munmap(mapped_region, map_length);
            return -1;
        }
        recs_in_portion = (portion_bytes - sizeof(uint64_t)) / sizeof(index_record);
    } else {
        start_offset = current_offset - map_offset;
        recs_in_portion = portion_bytes / sizeof(index_record);
    }
    if (portion_bytes % sizeof(index_record) != 0 && current_offset != 0) {
        fprintf(stderr, "Portion size not multiple of record size\n");
        munmap(mapped_region, map_length);
        return -1;
    }

    g_records = (index_record *)((char *)mapped_region + start_offset);

    if (recs_in_portion % g_block_count != 0) {
        fprintf(stderr, "Error: records in portion (%" PRIu64 ") not divisible by block count (%d)\n",
                recs_in_portion, g_block_count);
        munmap(mapped_region, map_length);
        return -1;
    }
    g_block_size = recs_in_portion / g_block_count;

    g_next_block = 0;
    pthread_barrier_init(&g_barrier, NULL, g_thread_count);

    pthread_t *tid_array = malloc(g_thread_count * sizeof(pthread_t));
    int *tid_arg = malloc(g_thread_count * sizeof(int));
    if (!tid_array || !tid_arg) {
        perror("malloc in process_portion");
        pthread_barrier_destroy(&g_barrier);
        munmap(mapped_region, map_length);
        return -1;
    }

    for (int i = 0; i < g_thread_count; i++) {
        tid_arg[i] = i;
        if (pthread_create(&tid_array[i], NULL, thread_sort, &tid_arg[i]) != 0) {
            perror("pthread_create in process_portion");
            free(tid_array);
            free(tid_arg);
            pthread_barrier_destroy(&g_barrier);
            munmap(mapped_region, map_length);
            return -1;
        }
    }
    for (int i = 0; i < g_thread_count; i++) {
        pthread_join(tid_array[i], NULL);
    }
    pthread_barrier_destroy(&g_barrier);
    free(tid_array);
    free(tid_arg);

    int current_block_count = g_block_count;
    while (current_block_count > 1) {
        g_next_block = 0;
        pthread_barrier_init(&g_barrier, NULL, g_thread_count);
        
        tid_array = malloc(g_thread_count * sizeof(pthread_t));
        tid_arg = malloc(g_thread_count * sizeof(int));
        if (!tid_array || !tid_arg) {
            perror("malloc in merge phase");
            pthread_barrier_destroy(&g_barrier);
            munmap(mapped_region, map_length);
            return -1;
        }
        for (int i = 0; i < g_thread_count; i++) {
            tid_arg[i] = i;
            if (pthread_create(&tid_array[i], NULL, thread_merge, &tid_arg[i]) != 0) {
                perror("pthread_create merge in process_portion");
                free(tid_array);
                free(tid_arg);
                pthread_barrier_destroy(&g_barrier);
                munmap(mapped_region, map_length);
                return -1;
            }
        }
        for (int i = 0; i < g_thread_count; i++) {
            pthread_join(tid_array[i], NULL);
        }
        pthread_barrier_destroy(&g_barrier);
        free(tid_array);
        free(tid_arg);
        
        current_block_count /= 2;
        g_block_count = current_block_count;
        g_block_size *= 2;
    }

    if (msync(mapped_region, map_length, MS_SYNC) < 0) {
        perror("msync in process_portion");
        munmap(mapped_region, map_length);
        return -1;
    }
    *portion_records_out = recs_in_portion;
    if (munmap(mapped_region, map_length) < 0) {
        perror("munmap in process_portion");
        return -1;
    }
    return 0;
}

int external_merge(const char *filename, run_boundary *runs, int run_count)
{
    while (run_count > 1) {
        int new_run_count = 0;
        for (int i = 0; i < run_count; i += 2) {
            if (i + 1 == run_count) {
                runs[new_run_count++] = runs[i];
            } else {
                off_t merge_offset = runs[i].offset;
                uint64_t total_recs = runs[i].nrecords + runs[i+1].nrecords;
                size_t merge_size = total_recs * sizeof(index_record);
                
                int pagesize = getpagesize();
                off_t map_offset = merge_offset - (merge_offset % pagesize);
                size_t map_length = merge_size + (merge_offset - map_offset);
                
                int fd = open(filename, O_RDWR);
                if (fd < 0) {
                    perror("open in external_merge");
                    return -1;
                }
                void *mapped_region = mmap(NULL, map_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_offset);
                if (mapped_region == MAP_FAILED) {
                    perror("mmap in external_merge");
                    close(fd);
                    return -1;
                }
                close(fd);
                index_record *ptr = (index_record *)((char *)mapped_region + (merge_offset - map_offset));
                merge_blocks(ptr, ptr + runs[i].nrecords, runs[i].nrecords, runs[i+1].nrecords);
                if (msync(mapped_region, map_length, MS_SYNC) < 0) {
                    perror("msync in external_merge");
                    munmap(mapped_region, map_length);
                    return -1;
                }
                munmap(mapped_region, map_length);
                runs[new_run_count].offset = runs[i].offset;
                runs[new_run_count].nrecords = total_recs;
                new_run_count++;
            }
        }
        run_count = new_run_count;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 5) {
        fprintf(stderr, "Usage: %s memsize blocks threads filename\n", argv[0]);
        return EXIT_FAILURE;
    }

    g_memsize = atoll(argv[1]);       
    int blocks = atoi(argv[2]);
    int threads = atoi(argv[3]);
    const char *filename = argv[4];

    int pagesize = getpagesize();
    if (g_memsize % pagesize != 0) {
        fprintf(stderr, "Error: memsize (%zu) is not a multiple of system page size (%d)\n", g_memsize, pagesize);
        return EXIT_FAILURE;
    }
    if (blocks <= 0 || (blocks & (blocks - 1)) != 0 || blocks < threads * 4) {
        fprintf(stderr, "Error: blocks must be a power of 2 and at least 4 times the number of threads\n");
        return EXIT_FAILURE;
    }
    g_block_count = blocks;
    g_thread_count = threads;

    int fd = open(filename, O_RDWR);
    if (fd < 0) {
        perror("open in main");
        return EXIT_FAILURE;
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat in main");
        close(fd);
        return EXIT_FAILURE;
    }
    if ((long unsigned int)st.st_size < sizeof(uint64_t)) {
        fprintf(stderr, "Error: file too small to contain header\n");
        close(fd);
        return EXIT_FAILURE;
    }
    if (read(fd, &g_total_records, sizeof(uint64_t)) != sizeof(uint64_t)) {
        fprintf(stderr, "Error reading header\n");
        close(fd);
        return EXIT_FAILURE;
    }
    close(fd);

    off_t data_start = sizeof(uint64_t);
    size_t file_data_size = st.st_size - data_start;
    
    int max_runs = (file_data_size + g_memsize - 1) / g_memsize; 
    run_boundary *runs = malloc(max_runs * sizeof(run_boundary));
    if (!runs) {
        perror("malloc runs");
        return EXIT_FAILURE;
    }
    int run_count = 0;
    off_t current_offset = data_start;
    
    while (current_offset < st.st_size) {
        size_t remaining = st.st_size - current_offset;
        size_t portion_bytes = (remaining >= g_memsize) ? g_memsize : remaining;
        if (current_offset != data_start && (portion_bytes % sizeof(index_record)) != 0) {
            fprintf(stderr, "Error: portion at offset %" PRIu64 " has size not multiple of record size\n", (uint64_t)current_offset);
            free(runs);
            return EXIT_FAILURE;
        }
        uint64_t portion_recs = 0;
        if (process_portion(filename, current_offset, portion_bytes, &portion_recs) != 0) {
            free(runs);
            return EXIT_FAILURE;
        }
        runs[run_count].offset = current_offset;
        runs[run_count].nrecords = portion_recs;
        run_count++;
        current_offset += portion_bytes;
    }
    
    printf("All portions processed. Total sorted runs: %d\n", run_count);
    if (run_count > 1) {
        if (external_merge(filename, runs, run_count) != 0) {
            free(runs);
            return EXIT_FAILURE;
        }
        printf("External merge complete. File is fully sorted.\n");
    } else {
        printf("Only one sorted run exists. File is fully sorted.\n");
    }
    
    free(runs);
    pthread_mutex_destroy(&g_block_mutex);
    return EXIT_SUCCESS;
}
