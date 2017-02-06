ifeq ($(OPT), O0)
	OPT=O0
else ifeq ($(OPT), O1)
	OPT=O1
else ifeq ($(OPT), O3)
	OPT=O3
else
	OPT=O2
endif

INC_DIR = .
#-DDMALLOC -DDMALLOC_FUNC_CHECK if dmalloc should be included
CXXFLAGS =	-std=c++11 -$(OPT) -g -Wall -fno-strict-aliasing \
			-fmessage-length=0 -I$(INC_DIR)

OBJS =	api/napsa.o \
		configuration.o \
		demux/demux.o \
		icn.o \
		icnproxythreadcleaner.o \
		ipsocket.o \
		main.o \
		monitoring/collector.o \
		monitoring/statistics.o \
		namespaces/namespaces.o \
		namespaces/ip.o \
		namespaces/http.o \
		namespaces/management.o \
		namespaces/management/dnslocal.o \
		namespaces/buffercleaners/ipbuffercleaner.o \
		namespaces/buffercleaners/httpbuffercleaner.o \
		proxies/http/httpproxy.o \
		proxies/http/tcpclient.o \
		proxies/http/tcpserver.o \
		trafficcontrol/trafficcontrol.o \
		trafficcontrol/dropping.o \
		transport/transport.o \
		transport/lightweight.o \
		transport/lightweighttimeout.o \
		transport/unreliable.o \
		types/eui48.o \
		types/icnid.o \
		types/ipaddress.o \
		types/netmask.o \
		types/nodeid.o \
		types/routingprefix.o

LIBS =	-lblackadder \
		-lboost_program_options \
		-lboost_thread \
		-lboost_system \
		-lconfig++ \
		-llog4cxx \
		-lmoly \
		-lnet \
		-lpcap \
		-lpthread

TARGET = nap

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all: $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
	
install:
	cp $(TARGET) /usr/bin
	mkdir -p /etc/nap
	mkdir -p /usr/share/doc/nap
	cp doc/nap.cfg /usr/share/doc/nap
	cp doc/nap.l4j /usr/share/doc/nap
	cp README.md /usr/share/doc/nap/README
	cp -aui doc/nap.cfg /etc/nap/
	cp -aui doc/nap.l4j /etc/nap/
	
uninstall:
	rm /usr/bin/$(TARGET)
	rm -rf /etc/nap
	rm -rf /usr/share/doc/nap
	rm /var/log/nap.log*
