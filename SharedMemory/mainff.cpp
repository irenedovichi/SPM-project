/*
Shared-memory version of the compressor/decompressor using pipeline and all-to-all building blocks of FastFlow.
FastFlow: https://github.com/fastflow/fastflow

Parallel schema:

               |     |--> R-Worker --|     
    L-Worker --|     |               |                     
               | --> |--> R-Worker --| --> Writer
    L-Worker --|     |               |                   
               |     |--> R-Worker --|    

Files are distinguished between "small files" and "big files" depending on BIGFILE_LOW_THRESHOLD.

Header: 
- Single block file: [1, size, cmp_size] --> 24 bytes 
- Big file splitted in N blocks: [N, size1, cmp_size1, ..., sizeN, cmp_sizeN] --> 8 + N * 16 bytes


Note: This file was built on top of the primes_a2a.cpp file from the exercises/spmcode7 folder, and the files in the ffc folder.
*/

#include <cstdio>
#include <iostream>
#include <string>

#include <cmdline_ff.hpp>

#include <ff/ff.hpp>
#include <ff/pipeline.hpp>
#include <ff/all2all.hpp>

using namespace ff;


// --------------------------------------------------------------------------------------------------- task --------------------
struct Task_t {
    Task_t(unsigned char *ptr, size_t size, const std::string &name):
        ptr(ptr), size(size), filename(name) {}

    unsigned char *ptr;                 // input pointer
    size_t size;                        // input size
    const std::string filename;         // source file name
    unsigned char *ptrOut = nullptr;    // output pointer
    size_t cmp_size = 0;                // output size
    size_t blockid = 1;                 // block identifier (for "BIG files")
    size_t nblocks = 1;                 // #blocks in which a "BIG file" is split
};


// --------------------------------------------------------------------------------------------------- L-worker ----------------
struct L_Worker: ff_monode_t<Task_t> {
    L_Worker(const std::vector<std::string> &files) : files(files) {}

    bool doWorkCompress(const std::string& fname, size_t size) {
        unsigned char *ptr = nullptr;

        if (QUITE_MODE >= 2) {
            std::fprintf(stderr, "L-Worker: %lu is compressing\n", get_my_id());
        }

		if (!mapFile(fname.c_str(), size, ptr)) return false;

        if (QUITE_MODE >= 2) {
            std::fprintf(stderr, "L-Worker: %lu has mapped the file\n", get_my_id());
        }

		if (size <= BIGFILE_LOW_THRESHOLD) {
			Task_t *t = new Task_t(ptr, size, fname);

            if (QUITE_MODE >= 2) {
                std::fprintf(stderr, "L-Worker: %lu is sending to the next stage the file %s\n", get_my_id(), fname.c_str());
            }

			ff_send_out(t); // sending to the next stage
		} else {
			const size_t fullblocks   = size / BIGFILE_LOW_THRESHOLD;
			const size_t partialblock = size % BIGFILE_LOW_THRESHOLD;
			for(size_t i = 0; i < fullblocks; ++i) {
				Task_t *t = new Task_t(ptr + (i * BIGFILE_LOW_THRESHOLD), BIGFILE_LOW_THRESHOLD, fname);
				t->blockid = i + 1;
				t->nblocks = fullblocks + (partialblock > 0);

                if (QUITE_MODE >= 2) {
                    std::fprintf(stderr, "L-Worker: %lu is sending to the next stage part %zd of the file %s\n", get_my_id(), t->blockid, fname.c_str());
                } 

				ff_send_out(t); // sending to the next stage
			}
			if (partialblock) {
				Task_t *t = new Task_t(ptr + (fullblocks * BIGFILE_LOW_THRESHOLD), partialblock, fname);
				t->blockid = fullblocks + 1;
				t->nblocks = fullblocks + 1;

                if (QUITE_MODE >= 2) {
                    std::fprintf(stderr, "L-Worker: %lu is sending to the next stage part %zd of the file %s that has size %ld\n", get_my_id(), t->blockid, fname.c_str(), partialblock);
                }

				ff_send_out(t); // sending to the next stage
			}
		}
		return true; 
    }

    bool doWorkDecompress(const std::string& fname, size_t size) { 
        unsigned char *ptr = nullptr;

        // Map the file into memory
        if (!mapFile(fname.c_str(), size, ptr)) return false;

        // Read the first 8 bytes to determine if it was a small or big file
        size_t nblocks = *reinterpret_cast<size_t *>(ptr);

        if (QUITE_MODE >= 2) {
            std::fprintf(stderr, "Num of blocks: %zu\n", nblocks);
        }

        // Initialize a pointer that scrolls the header until it reaches the actual file contents (8 bytes have already been read: nblocks)
        unsigned char *dataPtr = ptr + sizeof(size_t);  // Move past the 8 bytes of nblocks

        // Retrieve the original and compressed sizes of the blocks
        std::vector<size_t> Sizes(nblocks);
        std::vector<size_t> cmpSizes(nblocks);
        for (size_t i = 0; i < nblocks; ++i) {
            size_t sizeBlock = *reinterpret_cast<size_t *>(dataPtr);            
            size_t cmp_sizeBlock = *reinterpret_cast<size_t *>(dataPtr + sizeof(size_t)); 
            Sizes[i] = sizeBlock; 
            cmpSizes[i] = cmp_sizeBlock; 
            dataPtr += 2 * sizeof(size_t); // Move to the next block's size information

            if (QUITE_MODE >= 2) {
                std::fprintf(stderr, "Original size of block %zu: %zu, compressed size: %zu\n", i, sizeBlock, cmp_sizeBlock);
            }
        }

        // At this point, dataPtr points to the actual file contents (compressed data)
        size_t currentOffset = 0; // Offset to locate the data of each block

        for (size_t i = 0; i < nblocks; ++i) {
            size_t sizeBlock = Sizes[i]; // This will be used to prepare the buffer for the decompressed data
            size_t cmp_sizeBlock = cmpSizes[i];

            // Create a Task for this block
            Task_t *task = new Task_t(dataPtr + currentOffset, cmp_sizeBlock, fname);
            task->blockid = i + 1;    // Block identifier
            task->nblocks = nblocks; // Total number of blocks
            task->cmp_size = sizeBlock; // Original size of the block (will be the size of the decompressed data, i.e., the output size)

            // Adjust the offset for the next block
            currentOffset += cmp_sizeBlock;

            ff_send_out(task);
        }
        return true;
    }

    Task_t *svc(Task_t *) {
        // compress or decompress each file assigned to this worker
        for (const auto &file : files) {
            struct stat statbuf;

            if (stat(file.c_str(), &statbuf) == -1) {
                std::perror("stat failed");
                continue; 
            }

            if (comp) {
                // compress the file
                if (QUITE_MODE >= 2) {
                    std::fprintf(stderr, "L-Worker: %lu is compressing file %s of size %lld\n", get_my_id(), file.c_str(), statbuf.st_size);
                }

                if (!doWorkCompress(file, statbuf.st_size)) {
                    std::cerr << "Error compressing file: " << file << std::endl;
                    return EOS;
                }
            } else {
                // decompress the file
                if (!doWorkDecompress(file, statbuf.st_size)) {
                    std::cerr << "Error decompressing file: " << file << std::endl;
                    return EOS;
                }
            }
        }
        return EOS;
    }

    const std::vector<std::string> files;
};


// --------------------------------------------------------------------------------------------------- R-worker ----------------
struct R_Worker: ff_minode_t<Task_t> {
    R_Worker(const size_t Lw) : Lw(Lw) {}

    Task_t *svc(Task_t *in) {
        bool oneblockfile = (in->nblocks == 1);

        // Compression part
		if (comp) {
			unsigned char * inPtr  = in->ptr;	
			size_t          inSize = in->size;
			
			// get an estimation of the maximum compression size
			unsigned long cmp_len = compressBound(inSize);

            if (QUITE_MODE >= 2) {
                std::fprintf(stderr, "R-Worker %lu is compressing file %s, block %zd of size %zu. Bound of: %zu\n", get_my_id(), in->filename.c_str(), in->blockid, in->size, cmp_len);
            }

			// allocate memory to store compressed data in memory
			unsigned char *ptrOut = new unsigned char[cmp_len];
			if (compress(ptrOut, &cmp_len, (const unsigned char *)inPtr, inSize) != Z_OK) {
				if (QUITE_MODE >= 1) std::fprintf(stderr, "Failed to compress file in memory\n");
				success = false;
				delete [] ptrOut;
				delete in;
				return GO_ON;
			}

			in->ptrOut   = ptrOut;
			in->cmp_size = cmp_len;  // now it's the real compression size (see compress in miniz for details)

            if (QUITE_MODE >= 2) {
                std::fprintf(stderr, "R-Worker %lu has compressed file %s of size %zu of block %zd. True size of: %zu\n", get_my_id(), in->filename.c_str(), in->size, in->blockid, cmp_len);
            }

            if (!oneblockfile) {
                // Directly pass the task to the Writer
                ff_send_out(in);
                return GO_ON;
            } else { // Single block file case: write the compressed data to a file with header 
                std::string outfile{in->filename + SUFFIX};
                FILE *out_fp = std::fopen(outfile.c_str(), "wb");
                if (!out_fp) {
                    if (QUITE_MODE >= 1) 
                        std::fprintf(stderr, "Error opening file %s\n", outfile.c_str());
                    success = false;
                    delete [] in->ptrOut;
                    delete in;
                    return GO_ON;
                }
                
                // Write the header for the single block file: 1, size, cmp_size (24 bytes)
                std::fwrite(&in->nblocks, sizeof(in->nblocks), 1, out_fp);
                std::fwrite(&in->size, sizeof(in->size), 1, out_fp);
                std::fwrite(&in->cmp_size, sizeof(in->cmp_size), 1, out_fp);
                
                // Write the compressed data
                if (std::fwrite(in->ptrOut, 1, in->cmp_size, out_fp) != in->cmp_size) {
                    if (QUITE_MODE >= 1) 
                        std::fprintf(stderr, "Error writing compressed data to file %s\n", outfile.c_str());
                    success = false;
                }
                
                std::fclose(out_fp);
                
                if (REMOVE_ORIGIN) {
                    unlink(in->filename.c_str());
                }
                
                // Clean up and return
                unmapFile(in->ptr, in->size);	
                delete [] in->ptrOut;
                delete in;
                return GO_ON;
            }
		} else {
            // Decompression part

            // Prepare the buffer to store the decompressed data
            size_t buffer_size = in->cmp_size;
            unsigned char *buffer = new unsigned char[buffer_size];

            if (!oneblockfile) { 
                // The data to decompress is in the range: [in->ptr, in->ptr + in->size)
                unsigned char *temp_input_buffer = new unsigned char[in->size];
                std::memcpy(temp_input_buffer, in->ptr, in->size);

                if (uncompress(buffer, &buffer_size, temp_input_buffer, in->size) != Z_OK) {
                    if (QUITE_MODE >= 1) 
                        std::fprintf(stderr, "Error decompressing file %s\n", in->filename.c_str());
                    success = false;
                    delete [] buffer;
                    delete [] temp_input_buffer;
                    delete in;
                    return GO_ON;
                }
                delete [] temp_input_buffer;
                in->ptrOut = buffer;
                ff_send_out(in);
                return GO_ON;
            }
            // Single block file case: decompress the data pointed by in->ptr and write it to a file
            if (uncompress(buffer, &buffer_size, in->ptr, in->size) != Z_OK) {
                if (QUITE_MODE >= 1) 
                    std::fprintf(stderr, "Error decompressing file %s\n", in->filename.c_str());
                success = false;
                delete [] buffer;
                delete in;
                return GO_ON;
            }
            // Write the decompressed data to a file removing the suffix
            std::string outfile{in->filename.substr(0, in->filename.size() - strlen(SUFFIX))};
            FILE *out_fp = std::fopen(outfile.c_str(), "wb");
            if (!out_fp) {
                if (QUITE_MODE >= 1) 
                    std::fprintf(stderr, "Error opening file %s\n", outfile.c_str());
                success = false;
                delete [] buffer;
                delete in;
                return GO_ON;
            }
            // Write the decompressed data to the file
            if (std::fwrite(buffer, 1, buffer_size, out_fp) != buffer_size) {
                if (QUITE_MODE >= 1) 
                    std::fprintf(stderr, "Error writing decompressed data to file %s\n", outfile.c_str());
                success = false;
            }

            std::fclose(out_fp);

            if (REMOVE_ORIGIN) {
                unlink(in->filename.c_str());
            }
            // Clean up and return
            delete [] buffer;
            delete in;
            return GO_ON;
        }  
    }

    void svc_end() {
		if (!success) {
			if (QUITE_MODE>=1) std::fprintf(stderr, "R-Worker %ld: Exiting with (some) Error(s)\n", get_my_id());
			return;
		}
    }

    bool success = true;
    const size_t Lw;
};


// --------------------------------------------------------------------------------------------------- writer ------------------
struct Writer: ff_minode_t<Task_t> {
    Writer(const size_t Rw) : Rw(Rw) {}

    Task_t *svc(Task_t *in) {
        if (QUITE_MODE >= 2) {
            std::fprintf(stderr, "Welcome, I am the Writer and I received the block %zu of file %s\n", in->blockid, in->filename.c_str());
        }

        // Insert the current task into the map
        auto& fileEntry = fileMap[in->filename];
        fileEntry.first = in->nblocks;
        fileEntry.second.push_back(in);

        // Check if all blocks for the file are collected
        if (fileEntry.second.size() == fileEntry.first) {

            if (QUITE_MODE >= 2) {
                std::fprintf(stderr, "Writer: I have received all the blocks for file %s (last block was: %zu)\n", in->filename.c_str(), in->blockid);
            }

            // Sort blocks by block ID
            auto& blocks = fileEntry.second;
            std::sort(blocks.begin(), blocks.end(), [](Task_t* a, Task_t* b) {
                return a->blockid < b->blockid;
            });

            if (QUITE_MODE >= 2) {
                std::fprintf(stderr, "Writer: I have sorted the blocks for file %s\n", in->filename.c_str());
                for (auto& task : blocks) {
                    std::fprintf(stderr, "Block %zu\n", task->blockid);
                }
            }

            // Compression: write the compressed data to a file (adding the suffix) with the header
            if (comp) {
                // Write compressed file
                std::string outfile = blocks[0]->filename + SUFFIX;
                FILE* out_fp = std::fopen(outfile.c_str(), "wb");
                if (!out_fp) {
                    std::cerr << "Error opening file for writing: " << outfile << std::endl;
                    return GO_ON;
                }

                // Write first element of the header: nblocks
                std::fwrite(&blocks[0]->nblocks, sizeof(size_t), 1, out_fp);

                if (QUITE_MODE >= 2) {
                    std::fprintf(stderr, "Number of blocks is %zu\n", blocks[0]->nblocks);
                }

                // Write each block's size and compressed size
                for (auto& task : blocks) {
                    std::fwrite(&task->size, sizeof(size_t), 1, out_fp);
                    std::fwrite(&task->cmp_size, sizeof(size_t), 1, out_fp);
                }

                if (QUITE_MODE >= 2) {
                    for (auto& task : blocks) {
                        std::fprintf(stderr, "Original size %zu, cmp_size %zu\n", task->size, task->cmp_size);
                    }
                }

                // Write compressed data
                for (auto& task : blocks) {
                    std::fwrite(task->ptrOut, 1, task->cmp_size, out_fp);
                }

                std::fclose(out_fp);

            } else { // Decompression: write the decompressed data to a file (removing the suffix)
                std::string outfile = blocks[0]->filename.substr(0, blocks[0]->filename.size() - strlen(SUFFIX));
                FILE* out_fp = std::fopen(outfile.c_str(), "wb");
                if (!out_fp) {
                    std::cerr << "Error opening file for writing: " << outfile << std::endl;
                    return GO_ON;
                }

                // Write decompressed data
                for (auto& task : blocks) {
                    std::fwrite(task->ptrOut, 1, task->cmp_size, out_fp);
                    delete[] task->ptrOut;
                }

                std::fclose(out_fp);

            }

            // Remove original file if flag is set
            if (REMOVE_ORIGIN) {
                unlink(blocks[0]->filename.c_str());
            }

            fileMap.erase(in->filename);
        }

        if (QUITE_MODE >= 2) {
            std::fprintf(stderr, "Goodbye, I am the Writer and I have written the block %zu of file %s\n", in->blockid, in->filename.c_str());
        }

        return GO_ON;
    }

    void svc_end() {
        for (auto& [filename, fileData] : fileMap) {
            auto& blocks = fileData.second;
            for (auto task : blocks) {
                unmapFile(task->ptr, task->size);
                delete task;
            }
        }
        fileMap.clear();
    }

    // Key: filename, value: pair <nblocks, vector of blocks>
    std::unordered_map<std::string, std::pair<size_t, std::vector<Task_t*>>> fileMap;

    const size_t Rw;
};


// --------------------------------------------------------------------------------------------------- main --------------------
int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return -1;
    }
    // parse command line arguments and set some global variables
    long start = parseCommandLine(argc, argv);
    if (start < 0) return -1;

    const size_t Lw = lworkers;
    const size_t Rw = rworkers;

    // Start the timer (identical to chrono misurations)
    ffTime(START_TIME);

    // Distribute the workload among the Lw L-workers
    std::vector<std::vector<std::string>> partitions = partitionInput(start, argv, argc, Lw);

    // If quiet >= 1 print the partitions
    if (QUITE_MODE >= 1) {
        for (size_t i = 0; i < partitions.size(); ++i) {
            std::cout << "Partition " << i << ":\n";
            for (const auto& file : partitions[i]) {
                std::cout << file << std::endl;
            }
        }
    }

    // Define the FastFlow network ----------------------
    std::vector<ff_node*> LW;
    std::vector<ff_node*> RW;

    for (size_t i = 0; i < Lw; ++i) {
        LW.push_back(new L_Worker(partitions[i]));
    }

    for (size_t i = 0; i < Rw; ++i) {
        RW.push_back(new R_Worker(Lw));
    }

    ff_a2a a2a;
    a2a.add_firstset(LW);
    a2a.add_secondset(RW);

    Writer writer(Rw);

    ff_Pipe<> pipe(a2a, writer);

    pipe.blocking_mode(BLOCKING); 

    if (pipe.run_and_wait_end() < 0) {
        error("running pipe(a2a, writer)\n");
        return -1;
    }
    // --------------------------------------------------

    // Stop the timer
    ffTime(STOP_TIME);

    // Calculate and display the elapsed time in seconds (ffTime returns the elapsed time in milliseconds)
    std::cout << "Elapsed time: " << ffTime(GET_TIME) / 1000.0 << " s\n";

    if (QUITE_MODE >= 1) std::cout << "pipe(a2a, writer) Time: " << pipe.ffTime() << " (ms)\n";

    return 0;
}