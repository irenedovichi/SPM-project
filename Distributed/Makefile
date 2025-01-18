CXX		    = g++ -std=c++20
CXXMPI      = mpicxx -std=c++20
INCLUDES	= -I . -I miniz

LDFLAGS 	= -pthread
OPTFLAGS	= -O3 -ffast-math -DNDEBUG

TARGETS		= mainmpi

.PHONY: all clean cleanall
.SUFFIXES: .cpp 


%: %.cpp
	$(CXX) $(INCLUDES) $(OPTFLAGS) -o $@ $< ../miniz/miniz.c $(LDFLAGS)

all		: $(TARGETS)

mainmpi  : mainmpi.cpp cmdline_mpi.hpp utility_mpi.hpp
	$(CXXMPI) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< ../miniz/miniz.c $(LDFLAGS)


clean		: 
	rm -f $(TARGETS) 
cleanall	: clean
	\rm -f *.o *~