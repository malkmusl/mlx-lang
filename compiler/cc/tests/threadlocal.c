// Thread-local variables (_Thread_local, __thread): each thread's own,
// initialized from the program's TLS image; static, extern before the
// definition, over-aligned.
#include <stdio.h>
#include <pthread.h>
__thread void *cachePointer = NULL;
_Thread_local int counter = 5;
static __thread char small;
_Thread_local long aligned __attribute__((aligned(32))) = 7;
extern __thread int laterDefined;
int *counterAddress(void) { return &counter; }
static void *worker(void *arg) {
    long id = (long)arg;
    counter += (int)id;
    small = (char)id;
    cachePointer = arg;
    laterDefined = (int)id * 10;
    printf("thread %ld: counter %d small %d cache %ld aligned %ld later %d same %d mod32 %d\n", id, counter, small, (long)cachePointer, aligned, laterDefined, counterAddress() == &counter, (int)((unsigned long)&aligned % 32));
    return 0;
}
__thread int laterDefined = 3;
int main(void) {
    pthread_t t[3];
    for (long i = 0; i < 3; i++) { pthread_create(&t[i], 0, worker, (void *)(i + 1)); pthread_join(t[i], 0); }
    printf("main: counter %d small %d cache %p later %d\n", counter, small, cachePointer, laterDefined);
    return 0;
}
