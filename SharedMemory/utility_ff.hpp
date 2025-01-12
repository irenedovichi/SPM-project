/*
this file was built on top of the utility.hpp file from the ffc folder
*/

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <dirent.h> 
#include <sys/stat.h>
#include <ftw.h>

#include <algorithm>
#include <string>
#include <stdexcept>


#include <../miniz/miniz.h>


#define SUFFIX ".zip"
#define BUF_SIZE (1024 * 1024)

// global variables with their default values ---------------------------------------------------
static bool comp = true;                        // by default, it compresses 
static size_t BIGFILE_LOW_THRESHOLD = 2097152;  // 2Mbytes threshold
static bool REMOVE_ORIGIN = false;              // Does it keep the origin file?
static int  QUITE_MODE = 1;                     // 0 silent, 1 only errors, 2 everything
static bool RECUR = false;                      // do we have to process the contents of subdirs?
// ----------------------------------------------------------------------------------------------


// check if the string 's' is a number, otherwise it returns false
static bool isNumber(const char* s, long &n) {
    try {
		size_t e;
		n=std::stol(s, &e, 10);
		return e == strlen(s);
    } catch (const std::invalid_argument&) {
		return false;
    } catch (const std::out_of_range&) {
		return false;
    }
}