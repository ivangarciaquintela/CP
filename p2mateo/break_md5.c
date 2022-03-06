#include <sys/types.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>


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

    char *pass;
    unsigned char *md5;
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
    struct args *args = ptr;
    unsigned char *md5 = args->md5;
    unsigned char res[MD5_DIGEST_LENGTH];
    unsigned char *pass = malloc((PASS_LEN + 1) * sizeof(char));
    long i, j=0;
    while ((*args->ini) < BOUND)
    {
        pthread_mutex_lock(args->mtx);
        i = (*args->ini);
        (*args->ini) = (*args->ini) + 200;
        j= (*args->ini);
        pthread_mutex_unlock(args->mtx); 
        while (i != j)
        {
            //printf("i=%ld, j=%ld,ini=%ld thread=%d\n", i, j,  (*args->ini), args->thread_num) ;
            long_to_pass(i, pass);
            MD5(pass, PASS_LEN, res);
            if (0 == memcmp(res, md5, MD5_DIGEST_LENGTH))
            {
                pthread_mutex_lock(args->mtx);
                (*args->count) = BOUND;
                args->pass = (char *)pass;
                printf("\nresultado= %s", args->pass);
                fflush(stdout);

                pthread_mutex_unlock(args->mtx);
                break; // Found it!
            }
            else
            {
                pthread_mutex_lock(args->mtx);
                (*args->count)++;
                (*args->prob)++;
                pthread_mutex_unlock(args->mtx);
            }
            i++;
        }
    }

    return NULL;
}

void *progreso(void *ptr)
{
    struct args *args = ptr;

    while ((*args->count) != BOUND)
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
    return NULL;
}

struct thread_info *start_threads(void *operacion, void *ptr)
{

    struct thread_info *threads;
    struct opt *opt = ptr;


    if (operacion != progreso)
    {
        threads = malloc(sizeof(struct thread_info) * N_THREAD);
        for (int i = 0; i < N_THREAD; i++)
        {
            threads[i].args = malloc(sizeof(struct args));
            threads[i].args->md5 = opt->md5;
            threads[i].args->mtx = opt->mtx;
            threads[i].args->count = opt->count;
            threads[i].args->prob = opt->prob;
            threads[i].args->ini = opt->ini;
            threads[i].args->thread_num = i;

            if (0 != pthread_create(&threads[i].id, NULL, operacion, threads[i].args))
            {
                printf("Could not create thread\n");
                exit(1);
            }
        }
    }
    else
    {
        threads = malloc(sizeof(struct thread_info));

        if (threads == NULL)
        {
            printf("Not enough memory\n");
            exit(1);
        }

        threads->args = malloc(sizeof(struct args));
        threads->args->md5 = opt->md5;
        threads->args->mtx = opt->mtx;
        threads->args->count = opt->count;
        threads->args->prob = opt->prob;
        double t = tiempo();
        threads->args->timesys = t;

        if (0 != pthread_create(&threads->id, NULL, operacion, threads->args))
        {
            printf("Could not create thread\n");
            exit(1);
        }
    }
    return threads;
}
void waitT(struct thread_info *threads)
{
    // Wait for the threads to finish
    for (int i = 0; i < N_THREAD; i++)
        pthread_join(threads[i].id, NULL);
}

int main(int argc, char *argv[])
{
    struct thread_info *thrs=malloc(sizeof(pthread_t)*N_THREAD);
    struct thread_info *prg=malloc(sizeof(pthread_t));
    struct opt *opt;

    pthread_mutex_t *gl_it_mtx = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(gl_it_mtx, NULL);


    if (argc < 2)
    {
        printf("Use: %s string\n", argv[0]);
        exit(0);
    }

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
    opt->mtx = gl_it_mtx;

    prg = start_threads(progreso, opt);
    thrs = start_threads(break_pass, opt);
    waitT(thrs);

    free(thrs);
    free(prg);
    pthread_mutex_destroy(gl_it_mtx);
    return 0;
}