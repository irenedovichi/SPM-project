/*
Shared-memory version of the compressor/decompressor using pipeline and all-to-all building blocks of FastFlow.
FastFlow: https://github.com/fastflow/fastflow

Parallel schema:

               |     |--> R-Worker --|     
    L-Worker --|     |               |                     
               | --> |--> R-Worker --| --> Writer
    L-Worker --|     |               |                   
               |     |--> R-Worker --|    





* Parallel schema:
 *
 *  |<----------- FastFlow's farm ---------->| 
 *
 *               |---> Worker --->|
 *               |                |
 *   Reader ---> |---> Worker --->| --> Writer
 *               |                |
 *               |---> Worker --->|
 *
 *  "small files", those whose size is < BIGFILE_LOW_THRESHOLD
 *  "BIG files", all other files
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

};


struct Writer: ff_minode_t<Task_t> {

};


int main(int argc, char *argv[]) {

    return 0;
}