#include <sys/types.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#define PASS_LEN 6
#define N_THREAD 25
#define BOUND ipow(26, PASS_LEN)

struct thread_info
{
    pthread_t id;
    struct args *args;
};

struct args
{
    int thread_num;
    pthread_mutex_t *mtx;
    int nhashes;
    char *pass;
    unsigned char **md5;
    long *count;
    long *prob;
    long *ini;
    double timesys;
};

struct opt
{
    unsigned char *md5;
    long *count;
    long *prob;
    long *ini;
    pthread_mutex_t *mtx;
};

double tiempo()
{
    struct timeval t;
    if (gettimeofday(&t, NULL) < 0)
    {
        return 0.0;
    }
    return (t.tv_sec);
}

long ipow(long base, int exp)
{
    long res = 1;
    for (;;)
    {
        if (exp & 1)
            res *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }

    return res;
}

long pass_to_long(char *str)
{
    long res = 0;

    for (int i = 0; i < PASS_LEN; i++)
        res = res * 26 + str[i] - 'a';

    return res;
};

void long_to_pass(long n, unsigned char *str)
{ // str should have size PASS_SIZE+1
    for (int i = PASS_LEN - 1; i >= 0; i--)
    {
        str[i] = n % 26 + 'a';
        n /= 26;
    }
    str[PASS_LEN] = '\0';
}

int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    else
        return 0;
}

void hex_to_num(char *str, unsigned char *hex)
{
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
        hex[i] = (hex_value(str[i * 2]) << 4) + hex_value(str[i * 2 + 1]);
}

void *barra_progreso(void *ptr)
{
    struct args *args = ptr;
    int percent = ((*args->count) * 100) / BOUND;
    for (int i = 0; i < percent / 2; i++)
    {
        printf("=");
    }
    printf("%d %% (%ld)\r", percent, (*args->prob));
    (*args->prob) = 0;
    fflush(stdout);
    return NULL;
}

void *break_pass(void *ptr)
{
    unsigned char *aux;
    struct args *args = ptr;
    unsigned char **md5 = args->md5;
    unsigned char res[MD5_DIGEST_LENGTH];
    unsigned char *pass = malloc((PASS_LEN + 1) * sizeof(char));
    for (long i = args->thread_num; i < BOUND; i += N_THREAD)
    {
        pthread_mutex_lock(args->mtx);
        if (args->nhashes > 0)
        {

            long_to_pass(i, pass);

            MD5(pass, PASS_LEN, res);
            if ((*args->count) < BOUND)
            {
                (*args->count)++;
                (*args->prob)++;
                for (int y = 0; y < args->nhashes; y++)
                {
                    if (0 == memcmp(res, &md5[y], MD5_DIGEST_LENGTH))
                    {
                        for (int j = y; j < (args->nhashes - 1); j++)
                        {
                            aux = args->md5[j];
                            args->md5[j] = args->md5[j + 1];
                            args->md5[j + 1] = aux;
                        }
                        args->nhashes--;
                        args->pass = (char *)pass;
                        (*args->count) = BOUND;
                        break; // Found it!
                    }
                }
                pthread_mutex_unlock(args->mtx);
            }
        }
    }
    return NULL;
}

void *progreso(void *ptr)
{
    struct args *args = ptr;

    while ((*args->count) < BOUND)
    {
        double tn = tiempo();
        if (tn != args->timesys)
        {
            args->timesys = tn;
            pthread_mutex_lock(args->mtx);
            barra_progreso(args);
            pthread_mutex_unlock(args->mtx);
        }
    }
    pthread_mutex_lock(args->mtx);
    (*args->count) = BOUND;
    barra_progreso(args);
    pthread_mutex_unlock(args->mtx);
    return NULL;
}

void waitT(struct thread_info *threads)
{
    for (int i = 0; i < N_THREAD + 1; i++)
        pthread_join(threads[i].id, NULL);
}

void init_hashes(void *ptr, int nhashes, char *md5[])
{
    struct args *args = ptr;
    unsigned char md5_num[MD5_DIGEST_LENGTH];

    // inicializr mutex aqui ??

    args->md5 = malloc(sizeof(char *) * nhashes);

    for (int i = 0; i < nhashes; i++)
    {
        hex_to_num(md5[i], md5_num);
        args->md5[i] = md5_num;
    }
    args->nhashes = nhashes;
}

struct thread_info *start_threads(void *operacion, void *ptr, int argc, char *argv[])
{

    struct thread_info *threads;
    struct opt *opt = ptr;
    double t = tiempo();

    threads = malloc(sizeof(struct thread_info) * N_THREAD + 1);
    for (int i = 0; i < N_THREAD + 1; i++)
    {
        pthread_mutex_t *gl_it_mtx = malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(gl_it_mtx, NULL);

        threads[i].args = malloc(sizeof(struct args));
        threads[i].args->mtx = gl_it_mtx;
        threads[i].args->count = opt->count;
        threads[i].args->prob = opt->prob;
        threads[i].args->ini = opt->ini;
        threads[i].args->thread_num = i;
        init_hashes(threads[i].args,argc--,argv);


        if (i < N_THREAD)
        {
            if (0 != pthread_create(&threads[i].id, NULL, operacion, threads[i].args))
            {
                printf("Could not create thread\n");
                exit(1);
            }
        }
        else
        {
            threads[i].args->timesys = t;
            if (0 != pthread_create(&threads[i].id, NULL, progreso, threads[i].args))
            {
                printf("Could not create thread\n");
                exit(1);
            }
        }
    }
    return threads;
}

int main(int argc, char *argv[])
{
    struct thread_info *thrs = malloc(sizeof(pthread_t) * N_THREAD + 1);
    struct opt *opt;

    opt = malloc(sizeof(struct opt));
    long c = 0;
    long prob = 0;
    long ini = 0;

    unsigned char md5_num[MD5_DIGEST_LENGTH];

    hex_to_num(argv[1], md5_num);
    opt->count = &c;
    opt->ini = &ini;
    opt->prob = &prob;
    opt->md5 = md5_num;

    thrs = start_threads(break_pass, opt, argc, argv);
    waitT(thrs);
    printf("\n");
    for (int i = 0; i < N_THREAD; ++i)
    {
        if (thrs[i].args->pass != NULL)
            printf("%s: %s\n", argv[1], thrs[i].args->pass);
    }

    free(thrs);
    return 0;
}