#if !defined _CMDLINE_HPP_ff
#define _CMDLINE_HPP_ff

/*
this file was built on top of the cmdline.hpp file from the ffc folder
*/
#include <cstdio>
#include <string>

#include <ff/ff.hpp>

#include <utility_ff.hpp>


// Set some global variables, a few others are in utility_ff.hpp 
static long lworkers = 2;  // the number of Left Workers
static long rworkers = ff_numCores() - 3;  // the number of Right Workers
static bool BLOCKING = true;    // concurrency control, default is blocking


static inline void usage(const char *argv0) {
    std::printf("--------------------\n");
    std::printf("Usage: %s [options] file-or-directory [file-or-directory]\n", argv0);
    std::printf("\nOptions:\n");
    std::printf(" -l set the n. of Left Workers (default lworkers=%ld)\n", lworkers);
    std::printf(" -r set the n. of Right Workers (default rworkers=%ld)\n", rworkers);
    std::printf(" -t set the \"BIG file\" low threshold (in Mbyte -- min. and default %ld Mbyte)\n", BIGFILE_LOW_THRESHOLD /(1024 * 1024));
    std::printf(" -R 0 does not recur, 1 will process the content of all subdirectories (default R=%d)\n", RECUR ? 1 : 0);
    std::printf(" -C compress: 0 preserves, 1 removes the original file (default C=%d)\n", REMOVE_ORIGIN ? 1 : 0);
    std::printf(" -D decompress: 0 preserves, 1 removes the original file (default D=%d)\n", REMOVE_ORIGIN ? 1 : 0);
    std::printf(" -q 0 silent mode, 1 prints only error messages to stderr, 2 verbose (default q=%d)\n", QUITE_MODE ? 1 : 0);
    std::printf(" -b 1 blocking, 0 non-blocking concurrency control (default b=%d)\n", BLOCKING ? 1 : 0);
    std::printf("--------------------\n");
}

int parseCommandLine(int argc, char *argv[]) {
    extern char *optarg;
    const std::string optstr="l:r:t:R:C:D:q:b:";
    
    long opt, start = 1;
    bool cpresent = false, dpresent = false; // flags for the presence of -C and -D

    while((opt = getopt(argc, argv, optstr.c_str())) != -1) {
        switch(opt) {
            case 'l': {
                long l = 0;
                if (!isNumber(optarg, l)) {
                    std::fprintf(stderr, "Error: wrong '-l' option\n");
                    usage(argv[0]);
                    return -1;
                }
                // if l is negative or zero, it will be set to default value
                if (l <= 0) {
                    std::fprintf(stderr, "Warning: the number of Left Workers must be positive, set to default value %ld\n", lworkers);
                    l = lworkers;
                }
                lworkers = l;
                start += 2;      
            } break;
            case 'r': {
                long r = 0;
                if (!isNumber(optarg, r)) {
                    std::fprintf(stderr, "Error: wrong '-r' option\n");
                    usage(argv[0]);
                    return -1;
                }
                // if r is negative or zero, it will be set to default value
                if (r <= 0) {
                    std::fprintf(stderr, "Warning: the number of Right Workers must be positive, set to default value %ld\n", rworkers);
                    r = rworkers;
                }
                rworkers = r;
                start += 2;      
            } break;
            case 't': {
                long t = 0;
                if (!isNumber(optarg, t)) {
                    std::fprintf(stderr, "Error: wrong '-t' option\n");
                    usage(argv[0]);
                    return -1;
                }
                t = std::max(2l, t); // min threshold accepted is 2MB (2l = 2 long)
                BIGFILE_LOW_THRESHOLD = t * (1024 * 1024); // convert to bytes
                if (t > 100) { // just to set a limit
                    std::fprintf(stderr, "Error: \"BIG file\" low threshold too high, set it lower than 100 MB\n"); 
                    return -1;
                } 
                start += 2;
            } break;
            case 'R': {
                long n = 0;
                if (!isNumber(optarg, n)) {
                    std::fprintf(stderr, "Error: wrong '-R' option\n");
                    usage(argv[0]);
                    return -1;
                }
                if (n == 1) RECUR = true; 
                start += 2;	    
            } break;
            case 'C': {
                long c = 0;
                if (!isNumber(optarg, c)) {
                    std::fprintf(stderr, "Error: wrong '-C' option\n");
                    usage(argv[0]);
                    return -1;
                }
                cpresent = true;
                if (c == 1) REMOVE_ORIGIN = true; 
                start += 2; 
            } break;
            case 'D': {
                long d = 0;
                if (!isNumber(optarg, d)) {
                    std::fprintf(stderr, "Error: wrong '-D' option\n");
                    usage(argv[0]);
                    return -1;
                }	    
                dpresent = true;
                if (d == 1) REMOVE_ORIGIN = true; 
                comp = false; // comp = true in utility_ff.hpp, so by default it compresses 
                start += 2;
            } break;
            case 'q': {
                long q = 0;
                if (!isNumber(optarg, q)) {
                    std::fprintf(stderr, "Error: wrong '-q' option\n");
                    usage(argv[0]);
                    return -1;
                }	    	    
                QUITE_MODE = q;
                start += 2; 
            } break;
            case 'b': {
                long b = 0;
                if (!isNumber(optarg, b)) {
                    std::fprintf(stderr, "Error: wrong '-b' option\n");
                    usage(argv[0]);
                    return -1;
                }
                if (b == 0) BLOCKING = false;
                start += 2;
            } break;
            default:
                usage(argv[0]);
                return -1;
        }
    }
    if (cpresent && dpresent) {
		std::fprintf(stderr, "Error: -C and -D are mutually exclusive!\n");
		usage(argv[0]);
		return -1;
    }
    if ((argc - start) <= 0) {
		std::fprintf(stderr, "Error: at least one file or directory should be provided!\n");
		usage(argv[0]);
		return -1;
    }
    return start;
}

#endif // _CMDLINE_HPP_ff