#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
static void* quick(void* p) { return p; }
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t c = PTHREAD_COND_INITIALIZER;
static int stop;
static void* sleeper(void* p) { pthread_mutex_lock(&m); while (!stop) pthread_cond_wait(&c, &m); pthread_mutex_unlock(&m); return p; }
int main(int argc, char** argv)
{
    pthread_t a, b;
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("D0 mode=%s\n", argc > 1 ? argv[1] : "quick");
    pthread_create(&a, NULL, quick, NULL);
    pthread_detach(a);
    if (argc > 1) { pthread_create(&b, NULL, sleeper, NULL); pthread_detach(b); }
    usleep(200000);
    if (argc > 2) { printf("D2 cancel=%d\n", pthread_cancel(b)); }
    printf("D1 returning from main\n");
    return 0;
}
