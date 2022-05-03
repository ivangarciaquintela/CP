#include <stdio.h>
#include <sys/time.h>
#include <mpi.h>
#include <stdlib.h>

#define DEBUG 1

#define N 1024

int main(int argc, char *argv[] ) {

  int i, j, rank, numprocs;
  int rows;
  float matrix[N][N];
  float vector[N];
  float result[N];
  struct timeval  tv_t1, tv_t2, tv_c1, tv_c2;
  int tt, tc;

  MPI_Status status;
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  
  if(rank == 0){
     if(rank == 0){

       /* Initialize Matrix and Vector */
        for(i=0;i<N;i++) {
          vector[i] = i;
          for(j=0;j<N;j++) 
            matrix[i][j] = i+j;
          
        }
      }
  }

  gettimeofday(&tv_t1, NULL);
  MPI_Bcast(&vector, N, MPI_FLOAT, 0, MPI_COMM_WORLD);
  gettimeofday(&tv_t2, NULL);
  
  tt = (tv_t2.tv_usec - tv_t1.tv_usec) +  1000000 *(tv_t2.tv_sec - tv_t1.tv_sec);
  
  rows = (N+numprocs-1)/numprocs;

  float lMatrix[rows][N];
  float lResult[rows];


  gettimeofday(&tv_t1, NULL);
  MPI_Scatter(matrix, rows*N, MPI_FLOAT, lMatrix, rows*N, MPI_FLOAT, 0, MPI_COMM_WORLD);
  gettimeofday(&tv_t2, NULL);

  tt += (tv_t2.tv_usec - tv_t1.tv_usec) +  1000000 *(tv_t2.tv_sec - tv_t1.tv_sec);

  if(rank == numprocs-1)
    rows = N-rows*(numprocs-1);

  gettimeofday(&tv_c1, NULL);
  for(i=0;i<rows;i++) {
      lResult[i]=0;
      for(j=0;j<N;j++) {
        lResult[i] += lMatrix[i][j]*vector[j];
      }
  }
  gettimeofday(&tv_c2, NULL);


  gettimeofday(&tv_t1, NULL);
  //Gather result
  MPI_Gather(lResult, rows, MPI_FLOAT, result, rows, MPI_FLOAT, 0, MPI_COMM_WORLD);
  gettimeofday(&tv_t2, NULL);
  
  tt += (tv_t2.tv_usec - tv_t1.tv_usec) +  1000000 *(tv_t2.tv_sec - tv_t1.tv_sec);

  tc = (tv_c2.tv_usec - tv_c1.tv_usec) +  1000000 *(tv_c2.tv_sec - tv_t1.tv_sec);

  printf("Computational Time (seconds) of process %d = %lf\n", rank,(double) tc/1E6);

  printf("Transfer Time (seconds) of process %d = %lf\n", rank,(double) tt/1E6);


  //int microseconds = (tv_t2.tv_usec - tv_t1.tv_usec)+ 1000000 * (tv_t2.tv_sec - tv_t1.tv_sec);

  /*Display result */
  MPI_Barrier(MPI_COMM_WORLD);
  if (rank==0 && DEBUG){
    for(i=0;i<N;i++) {
      printf(" %f \t ",result[i]);
    }
  } else {
    printf ("\n");
  }    

  MPI_Finalize();
  return 0;
}
