/*
Sequential version of a compressor/decompressor using Miniz.

Miniz source code: https://github.com/richgel999/miniz
https://code.google.com/archive/p/miniz/
*/
/*
this file was built on top of the compdecomp.cpp file from the ffc folder
*/

#include <cstdio>
#include <chrono>
#include <iostream>

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

    bool success = true;

    // Start the timer
    auto start_time = std::chrono::steady_clock::now();

    // Process each file or directory provided in the command line
    for (int i = start; i < argc; ++i) {
        struct stat statbuf;
        if (stat(argv[i], &statbuf) == -1) {
            perror("stat");
            std::fprintf(stderr, "Error: stat %s\n", argv[i]);
            success = false;
            continue;
        }

        // Check if it's a directory or a file
        if (S_ISDIR(statbuf.st_mode)) { 
            success &= walkDir(argv[i], comp);
        } else {
            success &= doWork(argv[i], statbuf.st_size, comp); 
        }
    }

    // Stop the timer
    auto end_time = std::chrono::steady_clock::now();

    if (!success) {
        printf("Exiting with (some) Error(s)\n");
        return -1;
    }
    printf("Exiting with Success\n");

    // Calculate and display the elapsed time
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    std::cout << "Elapsed time: " << elapsed_seconds.count() << " s\n";

    return 0;
}