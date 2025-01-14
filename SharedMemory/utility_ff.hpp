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


// If compdecomp is true (we are compressing), it checks if fname has the suffix SUFFIX,
// if yes it returns true
// If compdecomp is false (we are decompressing), it checks if fname has the suffix SUFFIX,
// if yes it returns false
static inline bool discardIt(const char *fname, const bool compdecomp) {
    const int lensuffix=strlen(SUFFIX);
    const int len      = strlen(fname);
    if (len>lensuffix &&
		(strncmp(&fname[len-lensuffix], SUFFIX, lensuffix)==0)) {
		return compdecomp; // true or false depends on we are compressing or decompressing;
    }
    return !compdecomp;
}


// map the file pointed by filepath in memory
// if size is zero, it looks for file size
// if everything is ok, it returns the memory pointer ptr
static inline bool mapFile(const char fname[], size_t &size, unsigned char *&ptr) {
    // open input file.
    int fd = open(fname,O_RDONLY);
    if (fd<0) {
	if (QUITE_MODE>=1) {
	    perror("mapFile open");
	    std::fprintf(stderr, "Failed opening file %s\n", fname);
	}
	return false;
    }
    if (size==0) {
	struct stat s;
	if (fstat (fd, &s)) {
	    if (QUITE_MODE>=1) {
		perror("fstat");
		std::fprintf(stderr, "Failed to stat file %s\n", fname);
	    }
	    return false;
	}
	size=s.st_size;
    }

    // map all the file in memory
    ptr = (unsigned char *) mmap (0, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == MAP_FAILED) {
	if (QUITE_MODE>=1) {
	    perror("mmap");
	    std::fprintf(stderr, "Failed to memory map file %s\n", fname);
	}
	return false;
    }
    close(fd);
    return true;
}


// unmap a previously memory-mapped file
static inline void unmapFile(unsigned char *ptr, size_t size) {
    if (munmap(ptr, size)<0) {
	if (QUITE_MODE>=1) {
	    perror("nummap");
	    std::fprintf(stderr, "Failed to unmap file\n");
	}
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
            // It's a file, add it to the list
            files.push_back(fullPath);
        }
    }

    closedir(dir); // Close the directory after processing
    return files;
}


// Function to process a file without reading it (check suffix, size, and assign to a partition)
void processFile(const std::string &filePath, std::vector<std::vector<std::string>> &partitions, std::vector<long> &partitionSizes) {
    struct stat statbuf; // retrieves metadata about a file, including its size, without reading the file's content
    if (stat(filePath.c_str(), &statbuf) == -1) {
        return; // Skip if stat fails
    }

    // Check file size is non-zero
    if (statbuf.st_size == 0) {
        if (QUITE_MODE >= 2) {
            std::fprintf(stderr, "%s has size 0 -- ignored\n", filePath.c_str());
        }
        return;
    }

    // Check file suffix and discard if necessary
    if (comp && discardIt(filePath.c_str(), true)) {
        if (QUITE_MODE >= 2) {
            std::fprintf(stderr, "%s has already a %s suffix -- ignored\n", filePath.c_str(), SUFFIX);
        }
        return;
    }

    if (!comp && discardIt(filePath.c_str(), false)) {
        if (QUITE_MODE >= 2) {
            std::fprintf(stderr, "%s does not have a %s suffix -- ignored\n", filePath.c_str(), SUFFIX);
        }
        return;
    }

    // Find the partition with the smallest accumulated size
    auto min_partition = std::min_element(partitionSizes.begin(), partitionSizes.end());
    int partitionIndex = std::distance(partitionSizes.begin(), min_partition);

    // Assign the file to that partition
    partitions[partitionIndex].push_back(filePath);

    // Update the size of the selected partition
    *min_partition += statbuf.st_size;
}


// Distribute the workload among n workers
static std::vector<std::vector<std::string>> partitionInput(long start, char *argv[], int argc, int n) {
	// initialize n partitions
	std::vector<std::vector<std::string>> partitions(n);
	// number of files in each partition
	std::vector<long> partitionSizes(n, 0);

	for(long i = start; i < argc - start; ++i) {		
		struct stat statbuf; // retrieves metadata about a file, including its size, without reading the file's content

		// Check if it is a directory
		if (S_ISDIR(statbuf.st_mode)) {
			// Get all the files in the dir (depending on RECUR, get also the files in the subdirs)
			std::vector<std::string> files = getFilesInDir(argv[i], RECUR);

			// Process these files
            for (const std::string &file : files) {
                processFile(file, partitions, partitionSizes); 
            }

            continue;
		} 
		// argv[i] is a file
		processFile(argv[i], partitions, partitionSizes);
		}

	return partitions;
}