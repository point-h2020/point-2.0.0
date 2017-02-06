CXXFLAGS = -g -Wall -fmessage-length=0

LIBS = 	-lmoly -lboost_system -lboost_thread 

ALL =	client \
		server

all: $(ALL)

client:	client.cc
	$(CXX) $^ -o $@ $(LIBS)

server:	server.cc
	$(CXX) $^ -o $@ $(LIBS)
	
clean:
	rm -f $(ALL)
