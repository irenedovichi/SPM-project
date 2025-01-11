/*
Sequential version of a cmpressor/decompressor using Miniz.

Miniz source code: https://github.com/richgel999/miniz
https://code.google.com/archive/p/miniz/
*/

#include <cstdio>
#include <chrono>

#include <cmdline_seq.hpp>
#include <utility_seq.hpp>


int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return -1;
    }
    // parse command line arguments and set some global variables
    long start = parseCommandLine(argc, argv);
    if (start < 0) return -1;

    auto start_time = std::chrono::steady_clock::now();

    //TODO: Implement the sequential version of the compressor/decompressor

    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;

    // Output the time
    printf("Elapsed time: %f \n", elapsed_seconds);

    return 0;
}