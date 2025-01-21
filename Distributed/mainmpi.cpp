#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <map>

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

// --------------------------------------------------------------------------------------------------- datatype ----------------
// MPI datatype for MetaBlock
MPI_Datatype MPI_MetaBlock;
void createMetaBlockType() {
    MetaBlock block;

    int blocklengths[5] = {1, 256, 1, 1, 1};
    MPI_Datatype types[5] = {MPI_UNSIGNED_LONG, MPI_CHAR, MPI_UNSIGNED_LONG, MPI_UNSIGNED_LONG, MPI_UNSIGNED_LONG};
    MPI_Aint base, displacements[5];

    MPI_Get_address(&block, &base);
    MPI_Get_address(&block.size, &displacements[0]);
    MPI_Get_address(&block.filename, &displacements[1]);
    MPI_Get_address(&block.cmp_size, &displacements[2]);
    MPI_Get_address(&block.blockid, &displacements[3]);
    MPI_Get_address(&block.nblocks, &displacements[4]);

    for (int i = 0; i < 5; i++) {
        displacements[i] -= base;
    }

    MPI_Type_create_struct(5, blocklengths, displacements, types, &MPI_MetaBlock);
    MPI_Type_commit(&MPI_MetaBlock);
}

// --------------------------------------------------------------------------------------------------- main --------------------
int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int myId;
    int numP;
    MPI_Comm_rank(MPI_COMM_WORLD, &myId);
    MPI_Comm_size(MPI_COMM_WORLD, &numP);

    // Create the MPI datatype for MetaBlock
    createMetaBlockType();

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
    MPI_Bcast(&QUITE_MODE, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    //TODO: put a barrier before the timer starts????? --> MPI_Barrier(MPI_COMM_WORLD);
    // Start the timer
    double start_time = MPI_Wtime();

    if (!myId) {
        // The master creates the blocks of data and their metadata ------------------------------------------------------------------------
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

        // The master sends the metadata and the data to the other processes in a round robin fashion --------------------------------------
        size_t numWorkers = numP - 1;

        for (size_t i = 0; i < metaBlocks.size(); ++i) {
            size_t worker = i % numWorkers + 1;

            // Send the metadata
            MPI_Send(&metaBlocks[i], 1, MPI_MetaBlock, worker, 0, MPI_COMM_WORLD);

            // Send the data
            MPI_Send(dataBlocks[i].data(), dataBlocks[i].size(), MPI_CHAR, worker, 0, MPI_COMM_WORLD);
        }

        // Send termination message to all workers
        for (int dest = 1; dest <= numWorkers; ++dest) {
            MetaBlock terminationMeta;
            terminationMeta.size = -1; // Use -1 to indicate no more work
            MPI_Send(&terminationMeta, 1, MPI_MetaBlock, dest, 0, MPI_COMM_WORLD);
        }

        // The master gets the metadata and the data back from the workers ----------------------------------------------------------------
        std::map<std::string, std::vector<std::pair<MetaBlock, std::vector<char>>>> filesMap;

        // Receive all metadata and data from workers
        for (size_t i = 0; i < metaBlocks.size(); ++i) {
            // Receive the metadata
            MetaBlock receivedMetaBlock;
            MPI_Status status;
            MPI_Recv(&receivedMetaBlock, 1, MPI_MetaBlock, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);

            // Receive the data
            std::vector<char> receivedData(receivedMetaBlock.cmp_size);
            MPI_Recv(receivedData.data(), receivedData.size(), MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &status);

            // Add the metadata and the data to the map
            filesMap[receivedMetaBlock.filename].push_back({receivedMetaBlock, receivedData});

            if (QUITE_MODE >= 2) {
                std::fprintf(stderr, "Master received metadata block: size %zu, filename %s, cmp_size %zu, blockid %zu, nblocks %zu\n", receivedMetaBlock.size, receivedMetaBlock.filename, receivedMetaBlock.cmp_size, receivedMetaBlock.blockid, receivedMetaBlock.nblocks);
                std::fprintf(stderr, "Master received data block: size %zu\n", receivedData.size());
            }
        }

        // Save the processed data to files ------------------------------------------------------------------------------------------------
        for (auto& [filename, pairs] : filesMap) {

            if (QUITE_MODE >= 2) {
                std::fprintf(stderr, "Master has %lu pairs of (MetaBlock, data) for file %s\n", pairs.size(), filename.c_str());
            }

            // Sort the pairs by blockid
            std::sort(pairs.begin(), pairs.end(), [](const std::pair<MetaBlock, std::vector<char>>& a, const std::pair<MetaBlock, std::vector<char>>& b) {
                return a.first.blockid < b.first.blockid;
            });

            // Determine the output filename
            std::string outputFilename;
            if (comp) {
                outputFilename = filename + SUFFIX;
            } else {
                outputFilename = filename.substr(0, filename.size() - strlen(SUFFIX));
            }

            // Create the output file
            std::ofstream outFile(outputFilename, std::ios::binary);

            if (!outFile) {
                std::cerr << "Error opening file for writing: " << filename << std::endl;
                continue;
            }

            if (comp) { // Write the header in the compressed file
                size_t nblocks = pairs[0].first.nblocks;
                outFile.write(reinterpret_cast<char*>(&nblocks), sizeof(size_t));

                for (const auto& [metaBlock, _] : pairs) {
                    outFile.write(reinterpret_cast<const char*>(&metaBlock.size), sizeof(size_t));
                    outFile.write(reinterpret_cast<const char*>(&metaBlock.cmp_size), sizeof(size_t));
                }
            }

            // Write the data
            for (const auto& [_, data] : pairs) {
                outFile.write(data.data(), data.size());
            }

            outFile.close(); 

            // Remove original file if flag is set
            if (REMOVE_ORIGIN) {
                unlink(filename.c_str());
            }
        }
    } else {
        // The workers receive the metadata and the data from the master -------------------------------------------------------------------
        while (true) {
            // First probe for the message
            MPI_Status status;
            MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status); // Probe for a message from rank 0

            // Allocate memory for the MetaBlock
            MetaBlock receivedMetaBlock;
            // Receive the MetaBlock
            MPI_Recv(&receivedMetaBlock, 1, MPI_MetaBlock, 0, 0, MPI_COMM_WORLD, &status);

            // Check if the termination message was received
            if (receivedMetaBlock.size == -1) break;

            // Now that we have the metadata, we can proceed to receive the data
            std::vector<char> receivedData(receivedMetaBlock.size);
            // Receive the data block
            MPI_Recv(receivedData.data(), receivedData.size(), MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);

            if (QUITE_MODE >= 2) {
                std::fprintf(stderr, "Worker %d received metadata block: size %zu, filename %s, cmp_size %zu, blockid %zu, nblocks %zu\n", myId, receivedMetaBlock.size, receivedMetaBlock.filename, receivedMetaBlock.cmp_size, receivedMetaBlock.blockid, receivedMetaBlock.nblocks);
                std::fprintf(stderr, "Worker %d received data block: size %zu\n", myId, receivedData.size());
            }

            // Process the data block (compress or decompress) -----------------------------------------------------------------------------
            size_t size = receivedMetaBlock.size;
            size_t cmp_size = receivedMetaBlock.cmp_size;

            std::vector<char> sendData;

            if (comp) { 
                // get an estimation of the maximum compression size
                unsigned long cmp_len = compressBound(size);

                // allocate memory to store compressed data in memory
                unsigned char *ptrOut = new unsigned char[cmp_len];

                // compress the data
                if (compress(ptrOut, &cmp_len, reinterpret_cast<unsigned char*>(receivedData.data()), size) != Z_OK) {
                    std::cerr << "Process " << myId << " failed to compress the data" << std::endl;
                    delete [] ptrOut;
                    MPI_Abort(MPI_COMM_WORLD, -1);
                }

                if (QUITE_MODE >= 2) {
                    std::fprintf(stderr, "Worker %d has compressed the block %zu of file %s.\n", myId, receivedMetaBlock.blockid, receivedMetaBlock.filename);
                }

                // update cmp_size to cmp_len in receivedMetaBlock and save compressed data in the sendData vector
                receivedMetaBlock.cmp_size = cmp_len;
                std::memcpy(sendData.data(), ptrOut, cmp_len);

            } else {
                // allocate memory to store decompressed data in memory (cmp_size is the size of the original block)
                unsigned char *ptrOut = new unsigned char[cmp_size];

                // decompress the data
                if (uncompress(ptrOut, &cmp_size, reinterpret_cast<unsigned char*>(receivedData.data()), size) != Z_OK) {
                    std::cerr << "Process " << myId << " failed to decompress the data" << std::endl;
                    delete [] ptrOut;
                    MPI_Abort(MPI_COMM_WORLD, -1);
                }

                if (QUITE_MODE >= 2) {
                    std::fprintf(stderr, "Worker %d has decompressed the block %zu of file %s.\n", myId, receivedMetaBlock.blockid, receivedMetaBlock.filename);
                }

                // save decompressed data in the sendData vector
                std::memcpy(sendData.data(), ptrOut, cmp_size);
            }

            // Send the metadata and the data back to the master ---------------------------------------------------------------------------
            MPI_Send(&receivedMetaBlock, 1, MPI_MetaBlock, 0, 0, MPI_COMM_WORLD);
            MPI_Send(sendData.data(), sendData.size(), MPI_CHAR, 0, 0, MPI_COMM_WORLD);
        }
    }

    // Stop the timer
    double end_time = MPI_Wtime();

    // Calculate and display the elapsed time in seconds
    if(!myId){
    	std::cout << "Elapsed time: " << end_time - start_time << " s\n" << std::endl;
	}

    //TODO: Free the MPI datatype for MetaBlock and clear the vectors??????
    //MPI_Type_free(&MPI_MetaBlock);

    MPI_Finalize();
    return 0;
}