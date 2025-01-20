#include <cstdio>
#include <iostream>
#include <string>

#include <mpi.h>

#include <cmdline_mpi.hpp>


// --------------------------------------------------------------------------------------------------- block -------------------
struct MetaBlock { 
    size_t size;            // input size
    char filename[256];     // source file name
    size_t cmp_size;        // output size
    size_t blockid;         // block identifier (for "BIG files")
    size_t nblocks;         // #blocks in which a "BIG file" is split
};


// --------------------------------------------------------------------------------------------------- main --------------------
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

    // Broadcast the global variables //TODO: vedere se serve mandare queste variabili
    MPI_Bcast(&comp, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
    MPI_Bcast(&BIGFILE_LOW_THRESHOLD, 1, MPI_LONG, 0, MPI_COMM_WORLD);
    MPI_Bcast(&RECUR, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
    MPI_Bcast(&REMOVE_ORIGIN, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
    MPI_Bcast(&QUITE_MODE, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    //TODO: put a barrier before the timer starts????? --> MPI_Barrier(MPI_COMM_WORLD);
    // Start the timer
    double start_time = MPI_Wtime();

    /*TODO: the master creates the blocks, it will have 2 vectors: one of MetaBlock and one of char* (the files)
    - Comp case: takes the files and splits them into blocks depending on BIGFILE_LOW_THRESHOLD, fill the MetaBlock struct
    - Decomp case: takes the files, read the header and split the files into blocks, fill the MetaBlock struct

    Communication: the master takes component i of MetaBlock vector AND component i of char* vector and sends them to a process
    crea un buffer di totalSize
    use MPI_Send and MPI_Recv //TODO: valutare se vanno bene questi o serve il non bloccante
    il receiver usa MPI_Probe per sapere la size del messaggio
    */

    // the master gets the files to process and stores them in a vector
    if (!myId) {
        std::vector<std::string> files = getFiles(start, argv, argc);
        if (QUITE_MODE >= 2) {
            std::fprintf(stderr, "Master has %lu files to process\n", files.size());
        }
    }







    // Stop the timer
    double end_time = MPI_Wtime();

    // Calculate and display the elapsed time in seconds
    if(!myId){
    	std::cout << "Elapsed time: " << end_time - start_time << " s\n" << std::endl;
	}

    MPI_Finalize();
    return 0;
}