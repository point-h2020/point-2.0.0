CXX=g++
CXXFLAGS=-Wall -DDEBUG -std=c++11

OBJ = coap.o main.o clients.o coap_proxy.o logger.o  utils.o

TARGET = proxy

default: $(TARGET)

$(TARGET) : $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o proxy
main.o: main.c
	$(CXX) $(CXXFLAGS) $^ -c -o $@ $(LIBS) $(INCLUDE)
clients.o: clients.c
	$(CXX) $(CXXFLAGS) $^ -c -o $@ $(LIBS) $(INCLUDE)
coap_proxy.o: coap_proxy.c
	$(CXX) $(CXXFLAGS) $^ -c -o $@ $(LIBS) $(INCLUDE)
logger.o: logger.c
	$(CXX) $(CXXFLAGS) $^ -c -o $@ $(LIBS) $(INCLUDE)
coap.o: coap.cpp
	$(CXX) $(CXXFLAGS) $^ -c -o $@ $(LIBS) $(INCLUDE)
utils.o: utils.c
	$(CXX) $(CXXFLAGS) $^ -c -o $@ $(LIBS) $(INCLUDE)


clean:
	rm -f $(OBJ) $(TARGET) ; rm *~ 
