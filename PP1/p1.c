#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

int main(int argc, char *argv[])
{
    int i, done = 0, n, count, ccount, rank, size;
    double PI25DT = 3.141592653589793238462643;
    double pi, x, y, z;

    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    srand(rank+1);
    while (!done)
    {
        if (rank == 0)
        {
            printf("Enter the number of points: (0 quits) \n");
            scanf("%d", &n);
            for (int h = 1; h < size; h++)
            {
                MPI_Send(&n, 1, MPI_INT, h, 99, MPI_COMM_WORLD);
            }
        }
        else
        {
            MPI_Recv(&n, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
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

        if (rank != 0)
        {
            MPI_Send(&count, 1, MPI_INT, 0, 99, MPI_COMM_WORLD);
        }
        else
        {
            for (int p = 1; p < size; p++)
            {
                MPI_Recv(&ccount, 1, MPI_INT, p, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                count += ccount;
            }
            pi = ((double)count / (double)n) * 4.0;
            printf("pi is approx. %.16f, Error is %.16f\n", pi, fabs(pi - PI25DT));
        }
    }
    MPI_Finalize();
}