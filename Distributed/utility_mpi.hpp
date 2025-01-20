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

/*------------------------------------------------------------------------------------------------------------------------------------------------
Added by me 
------------------------------------------------------------------------------------------------------------------------------------------------*/

// Function to get files in a directory (recursively or not, depending on RECUR flag)
std::vector<std::string> getFilesInDir(const std::string &dirPath, bool RECUR) {
    std::vector<std::string> files;
    DIR *dir = opendir(dirPath.c_str());

    if (!dir) {
        std::cerr << "Error opening directory: " << dirPath << std::endl;
        return files; // Return an empty list on failure
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string fileName = entry->d_name;
        std::string fullPath = dirPath + "/" + fileName;

        // Skip "." and ".." directories
        if (fileName == "." || fileName == "..") {
            continue;
        }

        // Skip ".DS_Store" files
        if (fileName == ".DS_Store") {
            continue;
        }

        struct stat statbuf;
        if (stat(fullPath.c_str(), &statbuf) == -1) {
            continue; // Skip if stat fails
        }

        if (S_ISDIR(statbuf.st_mode)) {
            // If it's a directory and RECUR is true, recurse into the directory
            if (RECUR) {
                std::vector<std::string> subdirFiles = getFilesInDir(fullPath, RECUR);
                files.insert(files.end(), subdirFiles.begin(), subdirFiles.end());
            }
        } else {
            // It's a file, add it to the list with its size
            files.emplace_back(fullPath);
        }
    }
    closedir(dir); // Close the directory after processing

    return files;
}


// Function to get the files to process from the command line arguments
static std::vector<std::string> getFiles(long start, char *argv[], int argc) {
	std::vector<std::string> inputFiles;
	for (int i = start; i < argc; ++i) {
		struct stat statbuf; // retrieves metadata about a file, including its size, without reading the file's content

        if (stat(argv[i], &statbuf) == -1) {
            std::perror("stat failed");
            continue; 
        }

		// Check if it is a directory
		if (S_ISDIR(statbuf.st_mode)) {
            if (QUITE_MODE >= 2) {
                std::fprintf(stderr, "%s is a directory\n", argv[i]);
            }

			// Get all the files in the dir (depending on RECUR, get also the files in the subdirs)
			std::vector<std::string> files = getFilesInDir(argv[i], RECUR);

			// Add these files to the inputFiles vector
			for (const auto &file : files) {
				inputFiles.push_back(file);
			}

            continue;
		} else { // argv[i] is a file, add it to the inputFiles vector
			inputFiles.push_back(argv[i]);	
        }
	}
	return inputFiles;
}