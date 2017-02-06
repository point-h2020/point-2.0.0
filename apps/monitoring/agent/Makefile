CXXFLAGS = -std=c++11 -g -Wall -fmessage-length=0 #-DMONITORING_TRIGGER_BUGFIX

OBJS =	agent.o

LIBS = 	-lbampers\
		-lblackadder \
		-lboost_program_options \
		-lboost_system \
		-lboost_thread \
		-lmoly

TARGET = mona

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all: $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
