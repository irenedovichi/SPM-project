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



Note: This file was built on top of the primes_a2a.cpp file from the exercises/spmcode7 folder, and the files in the ffc folder.


--------------------
#TODO: scrivere in dettaglio questa parte
Compression:

Decompression:



 *
 * ------------
 * Compression: 
 * ------------
 * "small files" are memory-mapped by the Reader while they are
 * compressed and written into the FS by the Workers.
 *
 * "BIG files" are split into multiple independent files, each one 
 * having size less than or equal to BIGFILE_LOW_THRESHOLD, then
 * all of them will be compressed in by the Workers. Finally, all 
 * compressed files owning to the same "BIG file" are merged 
 * into a single file by using the 'tar' command. 
 * Reader: memory-map the input file, and splits it into multiple parts
 * Worker: compresses the assigned parts and then sends them to the Writer
 * Writer: waits for the compression of all parts and combine all of them 
 * together in a single file tar-file.
 *
 * --------------
 * Decompression:
 * --------------
 * The distinction between small and BIG files is done by checking
 * the header (magic number) of the file.
 *
 * "small files" are directly forwarded to the Workers that will 
 * do all the work (reading, decompressing, writing).
 *
 * "BIG files" are untarred into a temporary directory and then 
 * each part is sent to the Workers. The generic Worker decompresses
 * the assigned parts and then sends them to the Writer. 
 * The Writer waits for to receive all parts and then merges them
 * in the result file.
*/

#include <cstdio>
#include <iostream>
# include <string>

#include <cmdline_ff.hpp>
#include <utility_ff.hpp>

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
		if (!mapFile(fname.c_str(), size, ptr)) return false;
		if (size <= BIGFILE_LOW_THRESHOLD) {
			Task_t *t = new Task_t(ptr, size, fname);
			ff_send_out(t); // sending to the next stage
		} else {
			const size_t fullblocks   = size / BIGFILE_LOW_THRESHOLD;
			const size_t partialblock = size % BIGFILE_LOW_THRESHOLD;
			for(size_t i = 0; i < fullblocks; ++i) {
				Task_t *t = new Task_t(ptr + (i * BIGFILE_LOW_THRESHOLD), BIGFILE_LOW_THRESHOLD, fname);
				t->blockid = i + 1;
				t->nblocks = fullblocks + (partialblock > 0);
				ff_send_out(t); // sending to the next stage
			}
			if (partialblock) {
				Task_t *t = new Task_t(ptr + (fullblocks * BIGFILE_LOW_THRESHOLD), partialblock, fname);
				t->blockid = fullblocks + 1;
				t->nblocks = fullblocks + 1;
				ff_send_out(t); // sending to the next stage
			}
		}
		return true; 
    }

    bool doWorkDecompress(const std::string& fname, size_t size) { //TODO: adjust, this is a copy-paste from ffc/reader.hpp
        int r = checkHeader(fname.c_str());
		if (r < 0) { // fatal error in checking the header
            if (QUITE_MODE >= 1) 
                std::fprintf(stderr, "Error: checking header for %s\n", fname.c_str());
            return false;
        }
        if (r > 0) { // it was a small file compressed in the standard way
            ff_send_out(new Task_t(nullptr, size, fname)); // sending to one Worker
            return true;
        }
        // fname is a tar file (maybe), is was a "BIG file"       	
        std::string tmpdir;
        size_t found=fname.find_last_of("/");
        if (found != std::string::npos)
            tmpdir = fname.substr(0,found+1);
        else tmpdir = "./";

        // this dir will be removed in the Writer
        if (!createTmpDir(tmpdir))  return false;
        // ---------- TODO: this part should be improved
        std::string cmd = "tar xf " + fname + " -C" + tmpdir;
        if ((r = system(cmd.c_str())) != 0) {
            std::fprintf(stderr, "System command %s failed\n", cmd.c_str());
            removeDir(tmpdir, true);
            return false;
        }
        // ----------
        DIR *dir;
        if ((dir = opendir(tmpdir.c_str())) == NULL) {
            if (QUITE_MODE >= 1) {
                perror("opendir");
                std::fprintf(stderr, "Error: opening temporary dir %s\n", tmpdir.c_str());
            }
            removeDir(tmpdir, true);
            return false;
        }
        std::vector<std::string> dirV;
        dirV.reserve(200); // reserving some space
        struct dirent *file;
        bool error=false;
        while((errno = 0, file = readdir(dir)) != NULL) {
            std::string filename = tmpdir + "/" + file->d_name;
            if (!isdot(filename.c_str())) dirV.push_back(filename);
        }
        if (errno != 0) {
            if (QUITE_MODE >= 1) perror("readdir");
            error = true;
        }
        closedir(dir);
        size_t nfiles = dirV.size();
        for(size_t i = 0; i < nfiles; ++i) {
            Task_t *t = new Task_t(nullptr, 0, dirV[i]);
            t->blockid = i + 1;
            t->nblocks = nfiles;
            ff_send_out(t); // sending to the next stage	    
        }	
        return !error;
    }

    Task_t *svc(Task_t *) {
        // compress or decompress each file assigned to this worker
        for (const auto &file : files) {
            struct stat statbuf;
            if (comp) {
                // compress the file
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
		if (comp) {
			unsigned char * inPtr  = in->ptr;	
			size_t          inSize = in->size;
			
			// get an estimation of the maximum compression size
			unsigned long cmp_len = compressBound(inSize);

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

            if (!oneblockfile) {
                // Directly pass the task to the Writer
                ff_send_out(in);
                return GO_ON;
            } 
            // Single block file case: write the compressed data to a file with header = 1
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
            
            // Write the header
            unsigned char header = 1;
            if (std::fwrite(&header, sizeof(header), 1, out_fp) != 1) {
                if (QUITE_MODE >= 1) 
                    std::fprintf(stderr, "Error writing header to file %s\n", outfile.c_str());
                success = false;
                std::fclose(out_fp);
                delete [] in->ptrOut;
                delete in;
                return GO_ON;
            }
            
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
		// Decompression part

        
        ////////////////////////////////////////////////////
		bool remove = !oneblockfile || REMOVE_ORIGIN;
		if (decompressFile(in->filename.c_str(), in->size, remove) == -1) {
			if (QUITE_MODE>=1) 
				std::fprintf(stderr, "Error decompressing file %s\n", in->filename.c_str());
			success=false;
		}
		if (oneblockfile) {
			delete in;
			return GO_ON;
		}
		return in; // sending to the Writer
        ////////////////////////////////////////////////////
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
        
    }

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

    // Start the timer //TODO: eventualmente mettere chrono come in quello seq
    ffTime(START_TIME);

    // Distribute the workload among the Lw L-workers
    std::vector<std::vector<std::string>> partitions = partitionInput(start, argv, argc, Lw);

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

    pipe.blocking_mode(cc); //TODO: vedere se serve

    if (pipe.run_and_wait_end() < 0) {
        error("running pipe(a2a, writer)\n");
        return -1;
    }
    // --------------------------------------------------

    // Stop the timer
    ffTime(STOP_TIME);

    // Calculate and display the elapsed time in seconds (ffTime returns the elapsed time in milliseconds)
    std::cout << "Elapsed time: " << ffTime(GET_TIME) / 1000.0 << "s\n";

    if (QUITE_MODE >= 1) std::cout << "pipe(a2a, writer) Time: " << pipe.ffTime() << " (ms)\n";

    return 0;
}