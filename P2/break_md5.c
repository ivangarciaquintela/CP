#include <sys/types.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#define PASS_LEN 6

struct thread_info
{
    pthread_t id;      // id returned by pthread_create()
    struct args *args; // pointer to the arguments
};

struct args
{
    int thread_num; // application defined thread #
    int num_intentos;
    pthread_mutex_t * mtx;

};

int countup(int * it,pthread_mutex_t * mtx) {
  int temp;
  pthread_mutex_lock(mtx);
  //temp = (*it)++;       //esto esta mal
  pthread_mutex_unlock(mtx);
  return temp;
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

char *break_pass(unsigned char *md5, void *ptr, pthread_mutex_t * mtx)
{
    int j;
    struct args *args =  ptr;
    unsigned char res[MD5_DIGEST_LENGTH];
    unsigned char *pass = malloc((PASS_LEN + 1) * sizeof(char));
    long bound = ipow(26, PASS_LEN); // we have passwords of PASS_LEN
                                     // lowercase chars =>
                                     //     26 ^ PASS_LEN  different cases

    for (long i = 0; i < bound; i++)
    {
        long_to_pass(i, pass);

        MD5(pass, PASS_LEN, res);

        if (0 == memcmp(res, md5, MD5_DIGEST_LENGTH))
            break; // Found it!
        else{
            j = countup(&args->num_intentos, mtx);
            printf("%d\r", j);
        }
    }

    return (char *)pass;
}

void clean_progress(void)
{
    printf("\r \r");
}
void progress(int percent){
    printf("\r %3d ", percent);
    for (int i = 0; i < percent / 2; i++) { 
        printf("=");
        }
    if (percent < 100){
        printf(">\r");
    }
    else{
        printf("\r");
    }
}


void barra_progreso(/*void *ptr*/)
{
    //struct args *args = ptr;
    for (int i = 0; i < 101; i++)
    {
        progress(i);
        sleep(1);
    }
}

struct thread_info *start_threads(void *operacion)
{
    int i = 0;
    struct thread_info *threads;
    threads = malloc(sizeof(struct thread_info));
    if (threads == NULL)
    {
        printf("Not enough memory\n");
        exit(1);
    }
    if (0 != pthread_create(&threads[i].id, NULL, operacion, threads[i].args))
    {
        printf("Could not create thread #%d", i);
        exit(1);
    }
    return threads;
}

void freeT(struct thread_info *threads) {
	pthread_join(threads[0].id, NULL);
    free(threads[0].args);
    free(threads);	
	}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Use: %s string\n", argv[0]);
        exit(0);
    }
    struct thread_info *thrs;
    pthread_mutex_t * mtx = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(mtx,NULL);

    //thrs->args->num_intentos=0;
    //thrs->args->mtx = mtx;

    unsigned char md5_num[MD5_DIGEST_LENGTH];
    hex_to_num(argv[1], md5_num);
    thrs = start_threads(barra_progreso);
    char *pass = break_pass(md5_num, thrs->args, mtx);
    printf("%s: %s\n", argv[1], pass);
    free(pass);
	freeT(thrs);
    return 0;
}
