#include <mpi.h>



int main(int argc, char *argv[]) {
    
    MPI_Init(&argc, &argv);

    // put a barrier before the timer starts?? MPI_Barrier(MPI_COMM_WORLD);

    MPI_Finalize();
    return 0;
}