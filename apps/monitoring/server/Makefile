CXXFLAGS = -std=c++11 -g -Wall -fmessage-length=0

OBJS =	filesystemchecker.o \
		main.o \
		messagestack.o \
		mysqlconnector.o \
		types/scopedescriptor.o \
		scopes.o

LIBS = 	-lblackadder \
		-lbampers \
		-lboost_filesystem \
		-lboost_program_options \
		-lboost_system \
		-lboost_thread \
		-lconfig++ \
		-llog4cxx \
		-lmysqlclient \
		-lmysqlcppconn

TARGET = moose

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all: $(TARGET)

install:
	cp $(TARGET) /usr/bin
	mkdir -p /etc/moose
	mkdir -p /usr/share/doc/moose
	cp doc/moose.cfg /usr/share/doc/moose
	cp README.md /usr/share/doc/moose/README
	cp -aui doc/moose.cfg /etc/moose/
	mkdir -p /var/local/moose

uninstall:
	rm -rf /etc/moose
	rm -rf /usr/bin/moose
	rm -rf /usr/share/doc/moose
	rm -rf /var/local/moose

clean:
	rm -f $(OBJS) $(TARGET)
