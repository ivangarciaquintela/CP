#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "options.h"

#define MAX_AMOUNT 20

struct bank {
    int num_accounts;        // number of accounts
    int *accounts;           // balance array
    pthread_mutex_t *mutex;
};

struct args {
    int          thread_num;  // application defined thread #
    int          delay;       // delay between operations
    int	         *iterations;  // number of operations
    int          net_total;   // total amount deposited by this thread
    struct bank *bank;        // pointer to the bank (shared with other threads)
    pthread_mutex_t * itmtx;
};

struct thread_info {
    pthread_t    id;    // id returned by pthread_create()
    struct args *args;  // pointer to the arguments
};

int countdown(int * it,pthread_mutex_t * mtx) {
  int temp;
  pthread_mutex_lock(mtx);
  temp = *it ? (*it)-- : 0;
  pthread_mutex_unlock(mtx);
  return temp;
}
int countup(int * it,pthread_mutex_t * mtx) {
  int temp;
  pthread_mutex_lock(mtx);
  temp = *it ? (*it)++ : 0;
  pthread_mutex_unlock(mtx);
  return temp;
}

void lockAll(void *ptr){
    struct args *args =  ptr;
    int i;
    for(i=0;i>args->bank->num_accounts;i--){
        pthread_mutex_lock(&args->bank->mutex[i]);
    }
}

void unlockAll(void *ptr){
    struct args *args =  ptr;
    int i;
    for(i=0;i>args->bank->num_accounts;i--){
        pthread_mutex_unlock(&args->bank->mutex[i]);
    }
}

void balance_total(int it,pthread_mutex_t * mtx,void *ptr){
    struct args *args =  ptr;
    int i;
    int total=0;
    //lockAll(args);
    pthread_mutex_lock(mtx);
    printf("-----BALANCE TOTAL-----\n");
        for(i=0;i<args->bank->num_accounts;i++){
            printf("Cuenta %d  : %d \n",i, args->bank->accounts[i]);
            total += args->bank->accounts[i];
        }
    printf("Balance total = %d \n", total);
    printf("-----------------------\n");
    pthread_mutex_unlock(mtx);
    //unlockAll(args);
}

// Threads run on this function
void *deposit(void *ptr)
{
    struct args *args =  ptr;
    int amount, account, balance;

    while(countdown(args->iterations,args->itmtx)) {
        amount  = rand() % MAX_AMOUNT;
        account = rand() % args->bank->num_accounts;
        
        pthread_mutex_lock(&args->bank->mutex[account]);
        printf("Thread %d depositing %d on account %d\n",
            args->thread_num, amount, account);
        balance = args->bank->accounts[account];
        if(args->delay) usleep(args->delay); // Force a context switch

        balance += amount;
        if(args->delay) usleep(args->delay);

        args->bank->accounts[account] = balance;
        if(args->delay) usleep(args->delay);

        args->net_total += amount;
        pthread_mutex_unlock(&args->bank->mutex[account]);
    }
    return NULL;
}

void *transaccion(void *ptr)
{
    struct args *args =  ptr;
    int amount, acc1, acc2, balance1, balance2;

    while(countdown(args->iterations,args->itmtx)) {
        //printf("%d\n", *args->iterations);
        acc1 = rand() % args->bank->num_accounts;
        while (args->bank->accounts[acc1] < 1){
			acc1 = rand() % args->bank->num_accounts;
			}
        acc2 = rand() % args->bank->num_accounts;
        while (acc1 == acc2){
			acc2 = rand() % args->bank->num_accounts;
		}
        pthread_mutex_lock(&args->bank->mutex[acc1]);
        //IB: HOLD AND WAIT 
        if (pthread_mutex_trylock(&args->bank->mutex[acc2])){
		pthread_mutex_unlock(&args->bank->mutex[acc1]);
        countup(args->iterations,args->itmtx);
		continue;
		}
		amount  = rand() % args->bank->accounts[acc1];
		printf("Thread %d depositing %d for account %d on account %d\n",
        args->thread_num, amount,acc1, acc2);

        balance_total(*args->iterations, args->itmtx,args);
        balance1 = args->bank->accounts[acc1];
     
        if(args->delay) usleep(args->delay); // Force a context switch

        balance1 -= amount;
        
        if(args->delay) usleep(args->delay);
        
        args->bank->accounts[acc1] = balance1;
        
        if(args->delay) usleep(args->delay);
        
        balance2 = args->bank->accounts[acc2];
        
        if(args->delay) usleep(args->delay);
        
        balance2 += amount;
        
        if(args->delay) usleep(args->delay);
        
        args->bank->accounts[acc2] = balance2;
        args->net_total += amount;
        
		pthread_mutex_unlock(&args->bank->mutex[acc1]);
		pthread_mutex_unlock(&args->bank->mutex[acc2]);
    }
    return NULL;
}
// start opt.num_threads threads running on deposit.
struct thread_info *start_threads(struct options opt, struct bank *bank, void *operacion)
{
    int i;
    struct thread_info *threads;

    printf("creating %d threads\n", opt.num_threads);
    threads = malloc(sizeof(struct thread_info) * opt.num_threads);
    
    pthread_mutex_t * gl_it_mtx = malloc(sizeof(pthread_mutex_t));
      pthread_mutex_init(gl_it_mtx,NULL);

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }
    int *global_iters = malloc(sizeof(int));
    if ((bank->mutex= malloc(sizeof(pthread_mutex_t)*(bank->num_accounts)))==NULL){
		printf("Not enough memory\n");
		exit(1);
	} //se crea el array de MUTEXs

	for(i=0; i<bank->num_accounts; i++)
		pthread_mutex_init(&bank->mutex[i], NULL);

    *global_iters = opt.iterations;

    // Create num_thread threads running deposit
    for (i = 0; i < opt.num_threads; i++) {
        threads[i].args = malloc(sizeof(struct args));
        
        threads[i].args -> thread_num = i;
        if(operacion==deposit){
			threads[i].args -> net_total  = 0;
		}
        threads[i].args -> bank       = bank;
        threads[i].args -> delay      = opt.delay;
        threads[i].args->iterations = &opt.iterations;
        threads[i].args -> itmtx = gl_it_mtx;
        

        if (0 != pthread_create(&threads[i].id, NULL, operacion, threads[i].args)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
        
    }
    return threads;
}

// Print the final balances of accounts and threads
void print_balances(struct bank *bank, struct thread_info *thrs, int num_threads) {
    int total_deposits=0, bank_total=0;
    printf("\nNet deposits by thread\n");

    for(int i=0; i < num_threads; i++) {
        printf("%d: %d\n", i, thrs[i].args->net_total);
        total_deposits += thrs[i].args->net_total;
    }
    printf("Total: %d\n", total_deposits);

    printf("\nAccount balance\n");
    for(int i=0; i < bank->num_accounts; i++) {
        printf("%d: %d\n", i, bank->accounts[i]);
        bank_total += bank->accounts[i];
    }
    printf("Total: %d\n", bank_total);
}

// wait for all threads to finish, print totals, and free memory
void freeT(struct options opt, struct bank *bank, struct thread_info *threads,int z) {
	
    for (int i = 0; i < opt.num_threads; i++)
        free(threads[i].args);

    free(threads);
    free(bank->accounts);
	
	}
void waitT(struct options opt, struct bank *bank, struct thread_info *threads,int z) {
    // Wait for the threads to finish
    for (int i = 0; i < opt.num_threads; i++)
        pthread_join(threads[i].id, NULL);
    
    print_balances(bank, threads, opt.num_threads);

}

// allocate memory, and set all accounts to 0
void init_accounts(struct bank *bank, int num_accounts) {
    bank->num_accounts = num_accounts;
    bank->accounts     = malloc(bank->num_accounts * sizeof(int));

    for(int i=0; i < bank->num_accounts; i++)
        bank->accounts[i] = 0;
    
}

int main (int argc, char **argv)
{
    struct options      opt;
    struct bank         bank;
    struct thread_info *thrs;

    srand(time(NULL));

    // Default values for the options
    opt.num_threads  = 5;
    opt.num_accounts = 10;
    opt.iterations   = 100;
    opt.delay        = 10;

    read_options(argc, argv, &opt);

    init_accounts(&bank, opt.num_accounts);

    thrs = start_threads(opt, &bank,deposit);
    waitT(opt, &bank, thrs,0);
    thrs = start_threads(opt, &bank,transaccion);
	waitT(opt,&bank,thrs,0);
	freeT(opt,&bank,thrs,0);

    return 0;
}
