#include <sys/types.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>



#define PASS_LEN 6
#define N_THREAD 5
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
    //long *found;
    long *nhashes;
    double timesys;
};

struct opt
{
    unsigned char *md5;
    long *count;
    long *prob;
    //long *found;
    long *nhashes;
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

void clean_progress()
{
    printf("                                    \r");
    fflush(stdout);
}

void hex_to_num(char *str, unsigned char *hex)
{
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
        hex[i] = (hex_value(str[i * 2]) << 4) + hex_value(str[i * 2 + 1]);
}

void *barra_progreso(void *ptr)
{
    struct args *args = ptr;
    if ((*args->nhashes)==0)
    {
        *args->count=BOUND;    
    }
    
    int percent = ((*args->count) * 100) / BOUND;
    clean_progress();
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
    long i,q= 0;
    while((*args-> count)<BOUND){
        pthread_mutex_lock(args->mtx);
        i = (*args->ini);;
        (*args->ini)= (*args->ini)+100;
        q=(*args->ini)= (*args->ini);
        pthread_mutex_unlock(args->mtx);
        while (i != q)
        {
        long_to_pass(i, pass);
        MD5(pass, PASS_LEN, res);
        pthread_mutex_lock(args->mtx);
        if((*args->nhashes)>0){
            (*args->count)++;
            (*args->prob)++;
            if(0 == memcmp(res, md5, MD5_DIGEST_LENGTH)) {
                (*args->nhashes)=(*args->nhashes)-1;
                barra_progreso(args);
                printf("\n\r%s\n%ld hashes left\n", (char *)pass, (*args->nhashes));
                pthread_mutex_unlock(args->mtx);
                break; // Found it!
            }else{
            pthread_mutex_unlock(args->mtx);
            }
        }else{
            pthread_mutex_unlock(args->mtx);
            break;
        }
        i++;
        }
    }

    return NULL;
}

void *progreso(void *ptr)
{
    struct args *args = ptr;

    while ((*args->nhashes) >0)
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
    double t = tiempo();

    threads = malloc(sizeof(struct thread_info) * N_THREAD+1);
    for (int i = 0; i < N_THREAD+1; i++)
    {
            pthread_mutex_t *gl_it_mtx = malloc(sizeof(pthread_mutex_t));
            pthread_mutex_init(gl_it_mtx, NULL);

            threads[i].args = malloc(sizeof(struct args));
            threads[i].args->md5 = opt->md5;
            threads[i].args->mtx = gl_it_mtx;
            threads[i].args->count = opt->count;
            threads[i].args->prob = opt->prob;
            //threads[i].args->found = opt->found;
            threads[i].args->ini = opt->ini;
            threads[i].args->nhashes = opt->nhashes;
            threads[i].args->thread_num = i;

            if(i<N_THREAD){
                if (0 != pthread_create(&threads[i].id, NULL, operacion, threads[i].args))
                {
                    printf("Could not create thread\n");
                    exit(1);
                }
            }else{
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
void waitT(struct thread_info *threads)
{
    // Wait for the threads to finish
    for (int i = 0; i < N_THREAD+1; i++)
        pthread_join(threads[i].id, NULL);
}



int main(int argc, char *argv[])
{
    struct thread_info *thrs=malloc(sizeof(pthread_t)*N_THREAD+1);
    struct opt *opt = malloc(sizeof(struct opt));



    if (argc < 2)
    {
        printf("Use: %s string\n", argv[0]);
        exit(0);
    }
   
    long c = 0;
    long p = 0;
    //long f = 0;
    long ini = 0;
    long nhashes=argc-1;
    printf("%ld hashes provided\n",nhashes);

    unsigned char md5_num[MD5_DIGEST_LENGTH];
    hex_to_num(argv[1], md5_num);
    opt->count = &c;
    //opt->found =&f;
    opt->prob = &p;
    opt->ini = &ini;
    opt->nhashes= &nhashes;
    opt->md5 = md5_num;

    thrs = start_threads(break_pass, opt);
    waitT(thrs);
    printf("\n");
    /*for (int i = 0; i < N_THREAD; ++i)
    {
        if(thrs[i].args->pass!=NULL)
            printf("%s: %s\n", argv[1], thrs[i].args->pass);
    }*/
    
    free(thrs);
    return 0;
}