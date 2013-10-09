
OS = $(shell uname)

RM = /bin/rm

ifeq ($(OS),Linux) # These are for Linux
	INSTR = info play_saw record_raw in_out play_raw twostreams call_saw call_inout call_twostreams
	CC = g++ -Wall -D__LINUX_OSS__# -g -pg -O3
	LIBRARY = -lpthread
#	CC = g++ -g -Wall -D__LINUX_ALSA__ # -g -pg -O3
#	LIBRARY = -lpthread -lasound
	INCLUDE = -I../
endif

ifeq ($(OS),IRIX) # These are for SGI
	INSTR = info play_saw record_raw in_out play_raw twostreams call_saw call_inout call_twostreams
	CC = CC -D__IRIX_AL__ # -g -fullwarn -D__SGI_CC__ -O2
	LIBRARY = -laudio -lpthread
	INCLUDE = -I../
endif

all: $(INSTR)

clean :
	rm $(INSTR) *~ *.raw core *.o 

cleanIns : 
	rm $(INSTR)

strip : 
	strip $(INSTR)

info: info.cpp ../RtAudio.cpp ../RtAudio.h
	$(CC) -o info info.cpp ../RtAudio.cpp $(LIBRARY) $(INCLUDE)

play_saw: play_saw.cpp ../RtAudio.cpp ../RtAudio.h
	$(CC) -o play_saw play_saw.cpp ../RtAudio.cpp $(LIBRARY) $(INCLUDE)

play_raw: play_raw.cpp ../RtAudio.cpp ../RtAudio.h
	$(CC) -o play_raw play_raw.cpp ../RtAudio.cpp $(LIBRARY) $(INCLUDE)

record_raw: record_raw.cpp ../RtAudio.cpp ../RtAudio.h
	$(CC) -o record_raw record_raw.cpp ../RtAudio.cpp $(LIBRARY) $(INCLUDE)

in_out: in_out.cpp ../RtAudio.cpp ../RtAudio.h
	$(CC) -o in_out in_out.cpp ../RtAudio.cpp $(LIBRARY) $(INCLUDE)

twostreams: twostreams.cpp ../RtAudio.cpp ../RtAudio.h
	$(CC) -o twostreams twostreams.cpp ../RtAudio.cpp $(LIBRARY) $(INCLUDE)

call_saw: call_saw.cpp ../RtAudio.cpp ../RtAudio.h
	$(CC) -o call_saw call_saw.cpp ../RtAudio.cpp $(LIBRARY) $(INCLUDE)

call_inout: call_inout.cpp ../RtAudio.cpp ../RtAudio.h
	$(CC) -o call_inout call_inout.cpp ../RtAudio.cpp $(LIBRARY) $(INCLUDE)

call_twostreams: call_twostreams.cpp ../RtAudio.cpp ../RtAudio.h
	$(CC) -o call_twostreams call_twostreams.cpp ../RtAudio.cpp $(LIBRARY) $(INCLUDE)