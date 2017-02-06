#!/bin/bash
# this is a simple script to recompile everything useful for basic testing
# useful for a quick start or after changing branches
# not a replacement for reading the HowTo!
#set -x
#set -e
sudo apt install `cat apt-get.txt` --yes --force-yes
CORES=`nproc`
RAM=`cat /proc/meminfo | grep MemTotal | awk '{printf ("%.0f\n", $2/1000000)}'`
if [ $RAM -lt $CORES ]; then
	if [ $RAM -eq 0 ]; then
		CORES=1
	else
		CORES=$RAM
	fi
fi

sudo pkill -15 nap
sudo pkill -15 moose
# SRC
cd src/
#sudo make uninstall
autoconf
./configure --disable-linuxmodule
make clean
make -j$CORES
sudo make install
cd ..
# Blackadder library
cd lib/
autoreconf -fi
./configure
make clean
make -j$CORES
sudo make install
cd ..
# MOLY
cd lib/moly
make clean
make -j$CORES
sudo make install
# BAMPERS
cd ../bampers
make clean
make -j$CORES
sudo make install
cd ../mapi
## MAPI
make clean
make
sudo make install
cd ../..
### NAP
cd apps/nap
make clean
make -j$CORES
sudo make install
### Monitoring apps
cd ../monitoring/
cd agent
make clean
make
cd ../server
make clean
make -j$CORES
sudo make install
cd ../../../
# Topology manager
cd TopologyManager/
make clean > /dev/null 2>&1
make
cd ..
# Examples
cd examples/samples
make clean
make -j$CORES
cd ..
cd traffic_engineering/
make -j$CORES
cd ..
cd video_streaming/
make -j$CORES
cd ../surrogacy
make
cd ../../
cd deployment/
make clean > /dev/null 2>&1
make
cd ..
#cd examples/ipservices/gst
#make clean
#make -j$CORES
#cd ..

