/* Simple (and crappy) pthreads resource tracker for debugging resource leaks */
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdatomic.h>

#if !defined(__clang__) || !defined(__GNUC__)
#error Compiler not supported
#endif

static int (*_real_pthread_mutex_init)(pthread_mutex_t*, const pthread_mutexattr_t*);
static int (*_real_pthread_mutex_destroy)(pthread_mutex_t*);
static pthread_t (*_pthread_self)();

static void* hl = 0;

static atomic_uint count = 0;
static atomic_uint total = 0;

static int report_interval = 0;
static int mutex_limit = 0;
static int log_level = 0;

static void _report();

#define ERR(...) do { printf(__VA_ARGS__); abort(); } while(0)

static void __attribute__((constructor)) _init_hooks()
{
    static int initted = 0;
    if (initted)
        return;
    initted = 1;

    hl = dlopen("libpthread.so.0", RTLD_LAZY);
    if (!hl) {
        printf("Unable to open libpthread.so.0\n");
        abort();
    }
    _real_pthread_mutex_init = dlsym(hl, "pthread_mutex_init");
    _real_pthread_mutex_destroy = dlsym(hl, "pthread_mutex_destroy");
    _pthread_self = dlsym(hl, "pthread_self");
    if (!_real_pthread_mutex_destroy)
        ERR("Unable to find pthread_mutex_destroy");
    if (!_real_pthread_mutex_init)
        ERR("Unable to find pthread_mutex_init");
    if (!_pthread_self)
        ERR("Unable to find _pthread_self");

    char* p = getenv("MTRACK_MUTEX_LIMIT");
    if (p && (mutex_limit = atoi(p)) > 0)
        ;

    p = getenv("MTRACK_LOG_LEVEL");
    if (p && (log_level = atoi(p)) > 0)
        ;

    switch(log_level) {
    case 0:
    case 1:
        break;
    case 2:
        report_interval = 25; break;
    default:
        report_interval = 1; break;
    }


    p = getenv("MTRACK_REPORT_INTERVAL");
    if (p && (report_interval = atoi(p)) > 0)
        ;

    atexit(_report);
}

static void _report()
{
    dlclose(hl);
    fprintf(stderr, "%d total mutexes created, %d still alive at the end of execution\n", total, count);
}

int __attribute__((visibility("default"))) pthread_mutex_init (pthread_mutex_t *__mutex,
			       const pthread_mutexattr_t *__mutexattr)
{
    _init_hooks();

    if (mutex_limit && count >= mutex_limit) {
        fprintf(stderr, "mtrack: %s: mutex limit reached (%d)\n", __FUNCTION__, mutex_limit);
        return -EAGAIN;
    }

    count++;
    total++;
    if (report_interval == 1)
        fprintf(stderr, "mtrack: [thread 0x%lX] Create mutex\n", _pthread_self());
    if (report_interval > 1 && (total % report_interval) == 0)
        fprintf(stderr, "mtrack: Status: %d mutexes alive\n", count);
    return _real_pthread_mutex_init(__mutex, __mutexattr);
}

int __attribute__((visibility("default"))) pthread_mutex_destroy (pthread_mutex_t *__mutex)
{
    _init_hooks();
    count--;
    if (log_level > 1)
        fprintf(stderr, "mtrack: [thread 0x%lX] Destroy mutex\n", _pthread_self());
    return _real_pthread_mutex_destroy(__mutex);
}
