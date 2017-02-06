INCLUDE_DIR=/usr/local/include/moly
PRIMITIVES_DIR=/usr/local/include/moly/primitives
LIB_DIR=/usr/local/lib

LIBS = -lboost_system -lboost_thread 

CXXFLAGS = -fPIC -g -c -Wall

PRIMITIVES = primitives/addlink.hh \
			primitives/addnode.hh \
			primitives/addnodetype.hh \
			primitives/cmcgroupsize.hh \
			primitives/httprequestsperfqdn.hh \
			primitives/linkstate.hh \
			primitives/networklatencyperfqdn.hh \
			primitives/nodestate.hh
			
HEADERS = 	agent.hh \
			application.hh \
			enum.hh \
			moly.hh \
			primitives.hh \
			typedef.hh

OBJS =	agent.o \
		application.o \
		primitives/addlink.o \
		primitives/addnode.o \
		primitives/addnodetype.o \
		primitives/cmcgroupsize.o \
		primitives/httprequestsperfqdn.o \
		primitives/linkstate.o \
		primitives/networklatencyperfqdn.o \
		primitives/nodestate.o \
		moly.o

TARGET = libmoly.so

$(TARGET):	$(OBJS)
	$(CXX) -shared -rdynamic -Wl,-soname,$(TARGET) -o $(TARGET) $(OBJS) -lc $(LIBS)

install: $(PRIMITIVES) $(HEADERS)
	mkdir -p $(INCLUDE_DIR)
	mkdir -p $(PRIMITIVES_DIR)
	cp $(TARGET) $(LIB_DIR)
	cp $(HEADERS) $(INCLUDE_DIR)
	cp $(PRIMITIVES) $(PRIMITIVES_DIR)
	ldconfig
	
clean:
	rm -f $(OBJS) $(TARGET)

uninstall:
	make clean
	rm $(LIB_DIR)/$(TARGET)
	cd $(INCLUDE_DIR); rm $(HEADERS)
	cd $(PRIMITIVES_DIR); cd ../; rm $(PRIMITIVES); rm -rf primitives
	rm -rf $(INCLUDE_DIR)
	rm -rf $(PRIMITIVES_DIR)
	ldconfig