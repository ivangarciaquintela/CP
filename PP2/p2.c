#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

int ipow(int base, int exp)
{
    int result = 1;
    for (;;)
    {
        if (exp & 1)
            result *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }

    return result;
}

int MPI_BinomialColectiva(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm)
{
    int rank, size, i, a, rece, sen;
    MPI_Status status;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    for (i = 1; i <= ceil(log2(size)); i++)
        if (rank < ipow(2, i-1))
        {
            rece = rank + ipow(2, i-1);
            if (rece < size)
            {
                a = MPI_Send(buffer, count, datatype, rece, 0, comm);
                if (a != MPI_SUCCESS) return a;
            }
        } else
        {
            if (rank < ipow(2, i))
            {
                sen = rank - ipow(2, i-1);
                a = MPI_Recv(buffer, count, datatype, sen, 0, comm, &status);
                if (a != MPI_SUCCESS) return a;
            }
        }
    return MPI_SUCCESS;
}

int MPI_flatTreeColectiva(void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm)
{
    int rank, size, p, recbuf, a;
    int *ccount = recvbuf;
    MPI_Status status;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank != root)
    {
        MPI_Send(sendbuf, count, datatype, root, rank, comm);
    }
    else
    {
        *ccount = *(int *)sendbuf;
        for (p = 1; p < size; p++)
        {
            a=MPI_Recv(&recbuf, count, datatype, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &status);
            if (a!=MPI_SUCCESS){
                return a;
            }
            *ccount += recbuf;
        }
    }
    return MPI_SUCCESS;
}

int main(int argc, char *argv[])
{
    int i, done = 0, n, count;
    int ccount;
    int rank, size;
    double PI25DT = 3.141592653589793238462643;
    double pi, x, y, z;
    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    srand(rank + 1);
    while (!done)
    {
        if (rank == 0)
        {
            printf("Enter the number of points: (0 quits) \n");
            scanf("%d", &n);
        }
        //MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_BinomialColectiva(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (n == 0)
            break;

        count = 0;

        for (i = 1; i <= n; i += size)
        {
            // Get the random numbers between 0 and 1
            x = ((double)rand()) / ((double)RAND_MAX);
            y = ((double)rand()) / ((double)RAND_MAX);

            // Calculate the square root of the squares
            z = sqrt((x * x) + (y * y));

            // Check whether z is within the circle
            if (z <= 1.0)
                count++;
        }
        MPI_flatTreeColectiva(&count, &ccount, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        // MPI_Reduce(&count, &ccount, 1 , MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

        if (rank == 0)
        {
            pi = ((double)ccount / (double)n) * 4.0;
            printf("pi is approx. %.16f, Error is %.16f\n", pi, fabs(pi - PI25DT));
        }
    }
    MPI_Finalize();
}