INCLUDE_DIR=/usr/local/include/bampers
TYPES_DIR=/usr/local/include/bampers/types
DATAPOINTS_DIR=/usr/local/include/bampers/datapoints
LIB_DIR=/usr/local/lib

LIBS = 	-lblackadder \
		-lboost_thread
		
CXXFLAGS = -fPIC -g -c -std=c++11 -Wall

HEADERS = 	bampers.hh \
			cidanalyser.hh \
			icneventhandler.hh \
			enum.hh \
			namespace.hh \
			typedef.hh \

TYPES =	types/icnid.hh

DATAPOINTS =	datapoints/link.hh \
				datapoints/name.hh \
				datapoints/node.hh \
				datapoints/state.hh \
				datapoints/statistics.hh \

OBJS =	bampers.o \
		cidanalyser.o \
		icneventhandler.o \
		datapoints/link.o \
		datapoints/name.o \
		namespace.o \
		datapoints/node.o \
		datapoints/state.o \
		datapoints/statistics.o \
		types/icnid.o

TARGET = libbampers.so

$(TARGET):	$(OBJS)
	$(CXX) -shared -rdynamic -Wl,-soname,$(TARGET) -o $(TARGET) $(OBJS) $(LIBS) -lc

install: $(HEADERS)
	mkdir -p $(INCLUDE_DIR)
	mkdir -p $(INCLUDE_DIR)/datapoints
	mkdir -p $(INCLUDE_DIR)/types
	cp $(TARGET) $(LIB_DIR)
	cp $(HEADERS) $(INCLUDE_DIR)
	cp $(TYPES) $(TYPES_DIR)
	cp $(DATAPOINTS) $(DATAPOINTS_DIR)
	ldconfig
	
clean:
	rm -f $(OBJS) $(TARGET)

uninstall:
	make clean
	rm -f $(LIB_DIR)/$(TARGET)
	rm -f $(INCLUDE_DIR)
	ldconfig