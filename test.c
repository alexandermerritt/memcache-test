#include <pthread.h>
#include <libmemcached/memcached.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

const char config_string[] = "--SERVER=192.168.1.222 --BINARY-PROTOCOL";
const char testkey[] = "testkey";
const int keylen = sizeof(testkey);

bool thread_failure = false;
pthread_barrier_t barrier;

struct thread_args
{
    int count;
    size_t object_size;
    size_t *times;
};

static size_t inline
timespec2ns(const struct timespec *start, const struct timespec *end)
{
    size_t ns0 = start->tv_sec * 1e9 + start->tv_nsec;
    size_t ns1 = end->tv_sec * 1e9 + end->tv_nsec;
    return ns1 - ns0;
}

void*
thread(void *arg)
{
    struct thread_args *args = (struct thread_args*)arg;
    memcached_return_t mret;
    int count = 0;

    memcached_st *memc = memcached(config_string, strlen(config_string));
    if (!memc) {
        thread_failure = true;
        pthread_exit(NULL);
    }

    pthread_barrier_wait(&barrier);

    struct timespec start, end;
    do {
        size_t len;
        uint32_t flags;
        errno = 0;
        clock_gettime(CLOCK_REALTIME, &start);
        void *val = memcached_get(memc, testkey, keylen, &len, &flags, &mret);
        clock_gettime(CLOCK_REALTIME, &end);
        if (val) {
            args->times[count] = timespec2ns(&start, &end);
            free(val); // get creates storage for object each time
        } else {
            perror("memcached_get");
            thread_failure = true;
            break;
        }
    } while (++count < args->count);

    if (memc)
        memcached_free(memc);
    pthread_exit(NULL);
}

bool
run_test(int num_threads, size_t object_size, int count)
{
    struct thread_args *args = NULL;
    pthread_t *tids = NULL;
    bool oops = false;
    int i;

    pthread_barrier_init(&barrier, NULL, num_threads);

    // allocate threads
    tids = calloc(num_threads, sizeof(pthread_t*));
    args = calloc(num_threads, sizeof(*args));
    if (!tids || !args)
        return -1;

    for (i = 0; i < num_threads; i++) {
        args[i].times = calloc(count, sizeof(size_t));
        if (!args[i].times)
            goto out;
    }
    for (i = 0; i < num_threads; i++) {
        args[i].count = count;
        args[i].object_size = object_size;
        if (pthread_create(&tids[i], NULL, thread, &args[i]))
            break;
    }

    if (i < num_threads) {
        oops = true;
        while (i-- > 0)
            pthread_cancel(tids[i]);
    }
    if (oops)
        goto out;

    // block until threads are finished
    for (i = 0; i < num_threads; i++)
        pthread_join(tids[i], NULL);

    // print out all thread results
    if (!thread_failure) {
        for (int t = 0; t < num_threads; t++) {
            printf("%d", t);
            for (i = 0; i < count; i++)
                printf(" %lu", args[t].times[i]);
            printf("\n");
        }
    }

out:
    pthread_barrier_destroy(&barrier);
    for (i = 0; i < num_threads; i++)
        if (args[i].times)
            free(args[i].times);
    if (tids)
        free(tids);
    if (args)
        free(args);
    return thread_failure;
}

int
main(int argc, char *argv[])
{
    memcached_st *memc = NULL;
    void *testvalue = NULL;
    int ret = 0;
    int num_threads, object_size, count;

    if (argc == 4) {
        num_threads = atoi(argv[1]);
        object_size = atoi(argv[2]);
        count = atoi(argv[3]);
        if (num_threads < 1 || object_size < 1 || count < 1) {
            fprintf(stderr, "threads, size, count must be > 0\n");
            return -1;
        }
    } else {
        fprintf(stderr, "Usage: %s threads size count\n", *argv);
        return -1;
    }

    testvalue = malloc(object_size);
    if (!testvalue) {
        perror("allocating object");
        return -1;
    }

    memc = memcached(config_string, strlen(config_string));
    if (!memc)
        return -1;
    else {
        errno = 0;
        memcached_return_t mret = memcached_set(memc, testkey, keylen,
                                        testvalue, object_size, 0, 0);
        ret = (MEMCACHED_SUCCESS != mret);
        if (ret)
            perror("memcached_set");
        else {
            ret = run_test(num_threads, object_size, count);
            free(testvalue);
            testvalue = NULL;
        }
    }

    if (memc)
        memcached_free(memc);
    if (testvalue)
        free(testvalue);

    return ret;
}

