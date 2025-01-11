CXX		    = g++ -std=c++20
INCLUDES	= -I . -I miniz
OPTFLAGS	= -O3 -ffast-math -DNDEBUG 

TARGETS		= mainseq 

.PHONY: all clean cleanall
.SUFFIXES: .cpp 


%: %.cpp
	$(CXX) $(INCLUDES) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c

all		: $(TARGETS)

mainseq	: mainseq.cpp cmdline_seq.hpp utility_seq.hpp
	$(CXX) $(INCLUDES) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c 


clean		: 
	rm -f $(TARGETS) 
cleanall	: clean
	\rm -f *.o *~