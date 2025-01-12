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



This file was built on top of the primes_a2a.cpp file from the exercises/spmcode7 folder, and ffc_farm.cpp from the ffc folder.


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

#include <cmdline_ff.hpp>
#include <utility_ff.hpp>

#include <ff/ff.hpp>
#include <ff/pipeline.hpp>
#include <ff/all2all.hpp>

using namespace ff;


struct Task_t {

};


struct L_Worker: ff_monode_t<Task_t> {

};


struct R_Worker: ff_minode_t<Task_t> {
    R_Worker(const size_t Lw) : Lw(Lw) {}

    Task_t *svc(Task_t *in) {
        
    }

    const size_t Lw;
};


struct Writer: ff_minode_t<Task_t> {
    Writer(const size_t Rw) : Rw(Rw) {}

    Task_t *svc(Task_t *in) {
        
    }

    const size_t Rw;
};


// ---------------------------------------------------------- main ----------------------------------------------------------
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

    // Define the FastFlow network ----------------------
    std::vector<ff_node*> LW;
    std::vector<ff_node*> RW;

    for (size_t i = 0; i < Lw; ++i) {
        start = stop;
        stop = start + size + (more > 0 ? 1 : 0);
        --more;
        LW.push_back(new L_Worker(start, stop));
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