#if !defined _UTILITY_HPP_seq
#define _UTILITY_HPP_seq

/*
this file was built on top of the utility.hpp file from the ffc folder

Note: I modified the function compressFile in order to not compress files that already have SUFFIX as a suffix.
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
static bool REMOVE_ORIGIN = false;              // Does it keep the origin file?
static int  QUITE_MODE = 1;                     // 0 silent, 1 only errors, 2 everything
static bool RECUR = false;                      // do we have to process the contents of subdirs?
// ----------------------------------------------------------------------------------------------

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
// write size bytes starting from ptr into filename
static inline bool writeFile(const std::string &filename, unsigned char *ptr, size_t size) {
    FILE *pOutfile = fopen(filename.c_str(), "wb");
    if (!pOutfile) {
	if (QUITE_MODE>=1) {
	    perror("fopen");
	    std::fprintf(stderr, "Failed opening output file %s!\n", filename.c_str());
	}
	return false;
    }
    if (fwrite(ptr, 1, size, pOutfile) != size) {
	if (QUITE_MODE>=1) {
	    perror("fwrite");
	    std::fprintf(stderr, "Failed writing to output file %s\n", filename.c_str());
	}
	return false;
    }
    if (fclose(pOutfile) != 0) return false;
    return true;
}

// check if dir is '.' or '..'
static inline bool isdot(const char dir[]) {
  int l = strlen(dir);  
  if ( (l>0 && dir[l-1] == '.') ) return true;
  return false;
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
static inline char* getOption(char **begin, char **end, const std::string &option) {
    char **itr = std::find(begin, end, option);
    if (itr != end && ++itr != end) return *itr;
    return nullptr;
}
// create a tempory "unique" directory name
static inline bool createTmpDir(std::string& tmpdir) {
    tmpdir+="tmpdir.XXXXXX";
    char *tpl=const_cast<char*>(tmpdir.c_str());
    if (mkdtemp(tpl) == nullptr) {
	if (QUITE_MODE>=1) {
	    perror("mkdtemp");
	    std::fprintf(stderr, "Error: cannot create tmp dir %s\n", tmpdir.c_str());
	}
	return false;
    }
    return true;
}

// remove a file, it is used as a callback by removeDir
static int removeFile(const char *name, const struct stat *st=nullptr, int x=0, struct FTW *ftw=nullptr) {
    if (unlink(name) == -1) {
	if (QUITE_MODE>=1) {
	    perror("unlink");
	    std::fprintf(stderr, "Error: cannot remove file %s\n", name);
	}
	return -1;
    }
    return 0;
}	   
// remove tmpdir; if force, first removes all files and then the directory
static inline bool removeDir(const std::string &tmpdir, bool force=false) {
    
    if (rmdir(tmpdir.c_str()) == -1) {
	if (force && (errno == ENOTEMPTY || errno == EEXIST)) {
	    // Delete the directory and its contents by traversing the tree in reverse order,
	    // without crossing mount boundaries and symbolic links
	    if (nftw(tmpdir.c_str(),removeFile, 16, FTW_DEPTH|FTW_MOUNT|FTW_PHYS) < 0)  {
		if (QUITE_MODE>=1) {
		    perror("mkdtemp");
		    std::fprintf(stderr, "Error: cannot create tmp dir %s\n", tmpdir.c_str());
		}
		return false;
	    }	    
	    return true;
	}
	if (QUITE_MODE>=1) {
	    perror("rmdir");
	    std::fprintf(stderr, "Error: cannot remove tmp dir %s\n", tmpdir.c_str());
	}
	return false;	
    }
    return true;
}


// --------------------------------------------------------------------------
// compress the input file (fname) having size infile_size
// it returns 0 for success and -1 if something went wrong
// if removeOrigin is true, the source file will be removed if successfully compressed
static inline int compressFile(const char fname[], size_t infile_size,
			       const bool removeOrigin=REMOVE_ORIGIN) {
    // input file name
    const std::string infilename(fname);

	// Check if the file already has a .zip suffix
    if (infilename.size() > 4 && infilename.substr(infilename.size() - 4) == SUFFIX) {
        if (QUITE_MODE >= 1) {
            std::fprintf(stderr, "File %s already has a .zip suffix -- skipping compression\n", fname);
        }
        return 0; // Indicate success without compression
    }

	// define the output file name
    std::string outfilename = std::string(fname) + SUFFIX;

    unsigned char *ptr = nullptr;
    if (!mapFile(fname, infile_size, ptr)) return -1;
    // get an estimation of the maximum compression size
    unsigned long cmp_len = compressBound(infile_size);
    // allocate memory to store compressed data in memory
    unsigned char *ptrOut = new unsigned char[cmp_len];
    if (compress(ptrOut, &cmp_len, (const unsigned char *)ptr, infile_size) != Z_OK) {
	if (QUITE_MODE>=1) 
	    std::fprintf(stderr, "Failed to compress file in memory\n");
	delete [] ptrOut;
	return -1;
    }
    // write the compressed data into disk 
    bool success = writeFile(outfilename, ptrOut, cmp_len);
    if (success && removeOrigin) {
	removeFile(fname);
    }
    unmapFile(ptr, infile_size);
    delete [] ptrOut;
    return 0;
}

// returns -1 fatal error, 0 not valid header, 1 valid header
static inline int checkHeader(const char fname[]) {

    unsigned char s_inbuf[256];
    unsigned char s_outbuf[256];
    FILE *pInfile;
    // Open input file.
    pInfile = fopen(fname, "rb");
    if (!pInfile) return -1;

    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    stream.next_out  = s_outbuf;
    stream.avail_out = 256;

    if (inflateInit(&stream)) {
		fclose(pInfile); 
		return -1;
    }
    size_t n = 12;  // this is the minimum size of a compressed file
    if (fread(s_inbuf, 1, n, pInfile) != n) {   
		fclose(pInfile);
		return -1;
    }
    stream.next_in    = s_inbuf;
    stream.avail_in   = n;	  
    int status = inflate(&stream, Z_SYNC_FLUSH);
    fclose(pInfile);
    inflateEnd(&stream);
    if (status == Z_STREAM_END || status == Z_OK)  return 1;  
    return 0;
}
    
// uncompress the input file (fname) having size infile_size
// it returns 0 for success and -1 if something went wrong
// if removeOrigin is true, the input compressed file will be removed if successfully uncompressed
static inline int decompressFile(const char fname[], size_t infile_size,
				 const bool removeOrigin=REMOVE_ORIGIN) {
    unsigned char *s_inbuf  = new unsigned char[BUF_SIZE];
    unsigned char *s_outbuf = new unsigned char[BUF_SIZE];
    assert(s_inbuf); assert(s_outbuf);
    FILE *pInfile;
    FILE *pOutfile;
    size_t infile_remaining = 0;
    char *fnameOut = nullptr;
    int n=0;
    const std::string infilename(fname);
    std::string outfilename;
    
    // Open input file.
    pInfile = fopen(fname, "rb");
    if (!pInfile) {
	if (QUITE_MODE>=1) {
	    perror("fopen");
	    std::fprintf(stderr, "Failed opening input file!\n");
	}
	goto dcpError;;
    }
    // define the output file name
    // if the input file does not have a ".zip" extention
    // the output file name will terminate with "_decomp"
    // and the original file will not be removed even if
    // removeOrigin is true
    n = infilename.find(".zip");
    if (n>0) outfilename = infilename.substr(0,n);
    else     outfilename = infilename + "_decomp";
    fnameOut = const_cast<char *>(outfilename.c_str());

    z_stream stream;
    // Init the z_stream
    memset(&stream, 0, sizeof(stream));
    stream.next_in = s_inbuf;
    stream.avail_in = 0;
    stream.next_out = s_outbuf;
    stream.avail_out = BUF_SIZE;

    if (infile_size==0) {
	struct stat statbuf;
	if (stat(fname, &statbuf)==-1) {
	    if (QUITE_MODE>=1) {		
		perror("stat");
		std::fprintf(stderr, "Error: stat %s\n", fname);
	    }
	    goto dcpError;;
	}
	infile_size=statbuf.st_size;
    }
    infile_remaining = infile_size;
    if (inflateInit(&stream)) {
	if (QUITE_MODE>=1) 
	    std::fprintf(stderr, "inflateInit() failed!\n");
	goto dcpError;;
    }
    // Open output file.
    pOutfile = fopen(fnameOut, "wb");
    if (!pOutfile) {
	if (QUITE_MODE>=1) {
	    perror("fopen");
	    std::fprintf(stderr, "Failed opening output file!\n");
	}
	goto dcpError;;
    }    
    for ( ; ; ) {
      if (!stream.avail_in)   {
	  // Input buffer is empty, so read more bytes from input file.
	  size_t n = std::min((size_t)BUF_SIZE, infile_remaining);
	  if (fread(s_inbuf, 1, n, pInfile) != n) {
	      if (QUITE_MODE>=1) {
		  perror("fread");
		  std::fprintf(stderr, "Failed reading from input file!\n");
	      }
	      goto dcpError;;
	  }
	  stream.next_in    = s_inbuf;
	  stream.avail_in   = n;	  
	  infile_remaining -= n;
      }
      int status = inflate(&stream, Z_SYNC_FLUSH);
      if ((status == Z_STREAM_END) || (!stream.avail_out)) {
	  // Output buffer is full, or decompression is done, so write buffer to output file.
	  size_t n = BUF_SIZE - stream.avail_out;
	  if (fwrite(s_outbuf, 1, n, pOutfile) != n) {
	      if (QUITE_MODE>=1) {
		  perror("fwrite");
		  std::fprintf(stderr, "Failed writing to output file!\n");
	      }
	      goto dcpError;;
	  }
	  stream.next_out = s_outbuf;
	  stream.avail_out = BUF_SIZE;
      }      
      if (status == Z_STREAM_END)  break; // done
      else if (status != Z_OK) {
	  if (QUITE_MODE>=1)
	      std::fprintf(stderr, "inflate() failed with status %i!\n", status);
	  goto dcpError;;
      }
    }// for
    if (inflateEnd(&stream) != Z_OK) {
	if (QUITE_MODE>=1)
	    std::fprintf(stderr, "inflateEnd() failed!\n");
	goto dcpError;;
    }
    fclose(pInfile); fclose(pOutfile);
    if (n>0 && removeOrigin) removeFile(fname);
    delete [] s_inbuf;
    delete [] s_outbuf;
    return 0;
 dcpError:
    delete [] s_inbuf;
    delete [] s_outbuf;
    if (pInfile)  fclose(pInfile);
    if (pOutfile) fclose(pOutfile);
    return -1;
}
// --------------------------------------------------------------------------



// returns false in case of error
static inline bool doWork(const char fname[], size_t size, const bool comp) {
    if (comp) {
		if (compressFile(fname, size, REMOVE_ORIGIN) < 0) return false;
    } else {
		if (decompressFile(fname, size, REMOVE_ORIGIN) < 0) return false;
    }
    return true;
}

// returns false in case of error
static inline bool walkDir(const char dname[], const bool comp) {
    if (chdir(dname) == -1) {
		if (QUITE_MODE >= 1) {
			perror("chdir");
			std::fprintf(stderr, "Error: chdir %s\n", dname);
		}
		return false;
    }
    DIR *dir;	
    if ((dir = opendir(".")) == NULL) {
		if (QUITE_MODE >= 1) {
			perror("opendir");
			std::fprintf(stderr, "Error: opendir %s\n", dname);
		}
		return false;
    }
    struct dirent *file;
    bool error = false;
    while((errno = 0, file = readdir(dir)) != NULL) {
		if (strcmp(file->d_name, ".DS_Store") == 0) {
			continue; // Skip the .DS_Store file
		}
		struct stat statbuf;
		if (stat(file->d_name, &statbuf) == -1) {
			if (QUITE_MODE >= 1) {		
				perror("stat");
				std::fprintf(stderr, "Error: stat %s\n", file->d_name);
			}
			return false;
		}
		if (S_ISDIR(statbuf.st_mode)) {
			if ( !isdot(file->d_name) ) { // check if it is '.' or '..'
				if (RECUR) { // process the content of subdirs if RECUR is true
					if (walkDir(file->d_name, comp)) {
						if (chdir("..") == -1) {
							perror("chdir");
							std::fprintf(stderr, "Error: chdir ..\n");
							error = true;
						}
					} else error = true;
				}	
			}
		} else { // it is a file
			if (!doWork(file->d_name, statbuf.st_size, comp)) error = true;
		}
    }
    if (errno != 0) {
		if (QUITE_MODE >= 1) perror("readdir");
		error=true;
    }
    closedir(dir);
    return !error;
}

#endif // _UTILITY_HPP_seq
