# 
# FF_ROOT     pointing to the FastFlow root directory (i.e.
#             the one containing the ff directory).
ifndef FF_ROOT
FF_ROOT		= ../fastflow
endif

CXX		    = g++ -std=c++20
INCLUDES	= -I . -I miniz
CXXFLAGS  	+= -Wall -DBLOCKING_MODE #-DNO_DEFAULT_MAPPING -DFF_BOUNDED_BUFFER -DDEFAULT_BUFFER_CAPACITY=512 

LDFLAGS 	= -pthread
OPTFLAGS	= -O3 -ffast-math -DNDEBUG

TARGETS		= mainff

.PHONY: all clean cleanall
.SUFFIXES: .cpp 


%: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< ../miniz/miniz.c $(LDFLAGS)

all		: $(TARGETS)

mainff  : mainff.cpp cmdline_ff.hpp utility_ff.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(FF_ROOT) $(OPTFLAGS) -o $@ $< ../miniz/miniz.c $(LDFLAGS)


clean		: 
	rm -f $(TARGETS) 
cleanall	: clean
	\rm -f *.o *~