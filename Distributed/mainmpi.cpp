#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#include <mpi.h>

#include <cmdline_mpi.hpp>


// --------------------------------------------------------------------------------------------------- block -------------------
// Structure to store the metadata of a block, not the actual data because its size is not fixed
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

    if (!myId) {
        // Vectors to store metadata and data blocks
        std::vector<MetaBlock> metaBlocks;
        std::vector<std::vector<char>> dataBlocks;

        // the master gets the files (that have size != 0) to process, and stores them in a vector called `files`
        std::vector<std::string> files = getFiles(start, argv, argc);

        if (QUITE_MODE >= 2) {
            std::fprintf(stderr, "Master has %lu files to process\n", files.size());
        }
        
        // the master creates the blocks and fills the MetaBlock structs
        for (size_t i = 0; i < files.size(); i++) {
            struct stat statbuf;

            if (stat(files[i].c_str(), &statbuf) == -1) {
                std::perror("stat failed");
                continue;
            }

            if (comp) { // Compression: split the files into blocks depending on BIGFILE_LOW_THRESHOLD

                // Check if the file is already compressed (.zip)
                if (discardIt(files[i].c_str(), true)) {
                    if (QUITE_MODE >= 2) {
                        std::fprintf(stderr, "%s has already a %s suffix -- ignored\n", files[i].c_str(), SUFFIX);
                    }
                    continue;
                }

                size_t size = statbuf.st_size;

                if (size <= BIGFILE_LOW_THRESHOLD) { // Single block file
                    // Create a MetaBlock struct
                    MetaBlock block;
                    block.size = size;
                    std::strcpy(block.filename, files[i].c_str());
                    block.cmp_size = 0;
                    block.blockid = 1;
                    block.nblocks = 1;
                    // Add metadata to metaBlocks
                    metaBlocks.push_back(block);

                    // Read data into a buffer
                    std::vector<char> buffer(size);
                    std::ifstream inFile(files[i], std::ios::binary);
                    if (inFile) {
                        inFile.read(buffer.data(), size);
                        inFile.close();
                    }
                    // Add data to dataBlocks
                    dataBlocks.push_back(buffer);

                } else { // Multiple blocks file
                    const size_t fullblocks = size / BIGFILE_LOW_THRESHOLD;
                    const size_t partialblock = size % BIGFILE_LOW_THRESHOLD;

                    std::ifstream inFile(files[i], std::ios::binary);

                    if (!inFile) {
                        std::cerr << "Error opening file: " << files[i] << std::endl;
                        continue;
                    }
                    
                    for (size_t j = 0; j < fullblocks; ++j) {
                        // Create a MetaBlock struct
                        MetaBlock block;
                        block.size = BIGFILE_LOW_THRESHOLD;
                        std::strcpy(block.filename, files[i].c_str());
                        block.cmp_size = 0;
                        block.blockid = j + 1;
                        block.nblocks = fullblocks + (partialblock > 0);
                        // Add metadata to metaBlocks
                        metaBlocks.push_back(block);

                        // Read data into a buffer: only the data relative to the block!
                        std::vector<char> buffer(BIGFILE_LOW_THRESHOLD);

                        inFile.read(buffer.data(), BIGFILE_LOW_THRESHOLD);

                        if (QUITE_MODE >= 2) {
                            std::fprintf(stderr, "Buffer size: %zu\n", buffer.size());
                        }

                        // Add data to dataBlocks
                        dataBlocks.push_back(buffer);
                    }
                    if (partialblock) {
                        // Create a MetaBlock struct
                        MetaBlock block;
                        block.size = partialblock;
                        std::strcpy(block.filename, files[i].c_str());
                        block.cmp_size = 0;
                        block.blockid = fullblocks + 1;
                        block.nblocks = fullblocks + 1;
                        // Add metadata to metaBlocks
                        metaBlocks.push_back(block);

                        // Read data into a buffer: only the data relative to the block!
                        std::vector<char> buffer(partialblock);
                        inFile.read(buffer.data(), partialblock);

                        if(QUITE_MODE >= 2){
                            std::fprintf(stderr, "Buffer size: %zu\n", buffer.size());
                        }
                        
                        // Add data to dataBlocks
                        dataBlocks.push_back(buffer);
                    }
                    inFile.close();
                }
            } else { // Decompression: split the files into blocks depending on the header

                // Check if the file isn't a compressed file (no .zip)
                if (discardIt(files[i].c_str(), false)) {
                    if (QUITE_MODE >= 2) {
                        std::fprintf(stderr, "%s does not have a %s suffix -- ignored\n", files[i].c_str(), SUFFIX);
                    }
                    continue;
                }

                std::fstream inFile(files[i], std::ios::binary | std::ios::in);

                if (!inFile) {
                    std::cerr << "Error opening file: " << files[i] << std::endl;
                    continue;
                }

                // Read the first 8 bytes to determine if it was a small or big file (magic header)
                size_t nblocks;
                
                inFile.read(reinterpret_cast<char*>(&nblocks), sizeof(size_t));

                if (QUITE_MODE >= 2) {
                    //std::fprintf(stderr, "Decompressing file %s with %zu blocks\n", files[i].c_str(), nblocks);
                    std::cout << "Decompressing file " << files[i] << " with " << nblocks << " blocks" << std::endl; 
                }

                // Retrieve the original and compressed sizes of the blocks
                std::vector<size_t> Sizes(nblocks);
                std::vector<size_t> cmpSizes(nblocks);
                for (size_t j = 0; j < nblocks; ++j) {
                    inFile.read(reinterpret_cast<char*>(&Sizes[j]), sizeof(size_t));
                    inFile.read(reinterpret_cast<char*>(&cmpSizes[j]), sizeof(size_t));
                }

                // For each block, create a MetaBlock struct and add it to metaBlocks, and read the data into a buffer and add it to dataBlocks
                for (size_t j = 0; j < nblocks; ++j) {
                    // Create a MetaBlock struct
                    MetaBlock block;
                    block.size = cmpSizes[j]; // the size of the block is the compressed size
                    std::strcpy(block.filename, files[i].c_str());
                    block.cmp_size = Sizes[j];
                    block.blockid = j + 1;
                    block.nblocks = nblocks;
                    // Add metadata to metaBlocks
                    metaBlocks.push_back(block);

                    // Read data into a buffer: only the data relative to the block!
                    std::vector<char> buffer(cmpSizes[j]);
                    inFile.read(buffer.data(), cmpSizes[j]);
                    // Add data to dataBlocks
                    dataBlocks.push_back(buffer);
                }
                inFile.close();
            }

        }

        if (QUITE_MODE >= 2) {
            std::fprintf(stderr, "Master has created %lu meta blocks\n", metaBlocks.size());
        }
        if (QUITE_MODE >= 2) {
            std::fprintf(stderr, "Master has created %lu data blocks\n", dataBlocks.size());
        }

        // if quiet >= 2 print the sizes of each metadata block and the sizes of each data block
        if (QUITE_MODE >= 2) {
            for (size_t i = 0; i < metaBlocks.size(); ++i) {
                std::fprintf(stderr, "Metadata block %zu: size %zu, filename %s, cmp_size %zu, blockid %zu, nblocks %zu\n", i, metaBlocks[i].size, metaBlocks[i].filename, metaBlocks[i].cmp_size, metaBlocks[i].blockid, metaBlocks[i].nblocks);
            }
            for (size_t i = 0; i < dataBlocks.size(); ++i) {
                std::fprintf(stderr, "Data block %zu: size %zu\n", i, dataBlocks[i].size());
            }
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