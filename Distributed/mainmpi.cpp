#include <cstdio>
#include <iostream>
#include <string>

#include <mpi.h>

#include <cmdline_mpi.hpp>
#include <utility_mpi.hpp>


int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int myId;
    int numP;
    MPI_Comm_rank(MPI_COMM_WORLD, &myId);
    MPI_Comm_size(MPI_COMM_WORLD, &numP);

    if(argc < 2){
		// Only the first process prints the output message
		if(!myId){
			usage(argv[0]);
		}
		MPI_Abort(MPI_COMM_WORLD, -1);
        return -1;
	}

    // parse command line arguments and set some global variables
    int start = 0;
    if (!myId) {
        start = parseCommandLine(argc, argv);
        if (start < 0) {
            MPI_Abort(MPI_COMM_WORLD, -1);
            return -1;
        }
    }

    // Broadcast the global variables
    MPI_Bcast(&comp, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
    MPI_Bcast(&BIGFILE_LOW_THRESHOLD, 1, MPI_LONG, 0, MPI_COMM_WORLD);
    MPI_Bcast(&RECUR, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
    MPI_Bcast(&REMOVE_ORIGIN, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
    MPI_Bcast(&QUITE_MODE, 1, MPI_INT, 0, MPI_COMM_WORLD);
    




    //TODO: put a barrier before the timer starts????? --> MPI_Barrier(MPI_COMM_WORLD);
    // Start the timer
    double start_time = MPI_Wtime();

    // Stop the timer
    double end_time = MPI_Wtime();

    // Calculate and display the elapsed time in seconds
    if(!myId){
    	std::cout << "Elapsed time: " << end_time - start_time << " s\n" << std::endl;
	}

    MPI_Finalize();
    return 0;
}
