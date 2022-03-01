#include <sys/types.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PASS_LEN 6

struct thread_info
{
    pthread_t id;      // id returned by pthread_create()
    struct args *args; // pointer to the arguments
};

struct args
{
    int thread_num; // application defined thread #
    pthread_mutex_t * mtx;
    char *pass;
    unsigned char *md5;
    int percent;

};

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

long pass_to_long(char *str) {
    long res = 0;

    for(int i=0; i < PASS_LEN; i++)
        res = res * 26 + str[i]-'a';

    return res;
};

void long_to_pass(long n, unsigned char *str) {  // str should have size PASS_SIZE+1
    for(int i=PASS_LEN-1; i >= 0; i--) {
        str[i] = n % 26 + 'a';
        n /= 26;
    }
    str[PASS_LEN] = '\0';
}

int hex_value(char c) {
    if (c>='0' && c <='9')
        return c - '0';
    else if (c>= 'A' && c <='F')
        return c-'A'+10;
    else if (c>= 'a' && c <='f')
        return c-'a'+10;
    else return 0;
}

void hex_to_num(char *str, unsigned char *hex) {
    for(int i=0; i < MD5_DIGEST_LENGTH; i++)
        hex[i] = (hex_value(str[i*2]) << 4) + hex_value(str[i*2 + 1]);
}

void *barra_progreso(void *ptr)
{
    struct args *args = ptr;
    char *progress ="";
    for (int i = 0; i< args->percent; i++){
     progress = progress+".";
    }
    printf("%d % %s \r", args->percent, progress);
    
    
    return NULL;
}

void *break_pass(void * ptr) {
    struct args *args = ptr;
    unsigned char *md5 = args->md5;
    unsigned char res[MD5_DIGEST_LENGTH];
    unsigned char *pass = malloc((PASS_LEN + 1) * sizeof(char));
    long bound = ipow(10, PASS_LEN); // we have passwords of PASS_LEN
                                     // lowercase chars =>
                                    //     26 ^ PASS_LEN  different cases
    for(long i=0; i < bound; i++) {
        long_to_pass(i, pass);

        MD5(pass, PASS_LEN, res);

        if(0 == memcmp(res, md5, MD5_DIGEST_LENGTH)) {
            args-> percent = 100;
            barra_progreso(args);
            break; // Found it!
        }
        else{
            args->percent = (i*100)/bound;
            barra_progreso(args);
        }
    }
    args->pass = (char *)pass;
    return NULL;
}

struct thread_info *start_threads(void *operacion, unsigned char *md5)
{

    struct thread_info *threads;

    threads = malloc(sizeof(struct thread_info));
    
    pthread_mutex_t * gl_it_mtx = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(gl_it_mtx,NULL);

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }
    threads->args = malloc(sizeof(struct args));
    threads->args->mtx = gl_it_mtx;
    threads->args->md5 = md5;

    if (0 != pthread_create(&threads->id, NULL, operacion ,threads->args))
    {
        printf("Could not create thread\n");
        exit(1);
    }
    return threads;
}

int main(int argc, char *argv[]) {
    struct thread_info *thrs;

    if(argc < 2) {
        printf("Use: %s string\n", argv[0]);
        exit(0);
    }

    unsigned char md5_num[MD5_DIGEST_LENGTH];
    hex_to_num(argv[1], md5_num);
    thrs = start_threads(break_pass, md5_num);
    pthread_join(thrs->id, NULL);
    printf("\r");
    printf("%s: %s\n", argv[1], thrs->args->pass);
    free(thrs);
    return 0;
}