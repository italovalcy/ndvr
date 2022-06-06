NDVR
====

NDVR (NDN Distance Vector Routing) is routing protocol for NDN. More details soon.

Building
========

Custom version of NS-3 and specified version of ndnSIM needs to be installed.

The code should also work with the latest version of ndnSIM, but it is not guaranteed.

    mkdir ndnSIM
    cd ndnSIM

    git clone -b ndnSIM-ns-3.30.1 https://github.com/named-data-ndnSIM/ns-3-dev.git ns-3
    git clone -b 0.21.0 https://github.com/named-data-ndnSIM/pybindgen.git pybindgen
    git clone -b ndnSIM-2.8 --recursive https://github.com/named-data-ndnSIM/ndnSIM ns-3/src/ndnSIM

    # Build and install NS-3 and ndnSIM
    cd ns-3
    #./waf configure -d optimized
    ./waf configure -d debug
    ./waf
    sudo ./waf install

    # When using Linux, run
    sudo ldconfig


    cd ..
    git clone https://github.com/italovalcy/ndvr
    cd ndvr
    PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH ./waf configure --debug
    ./waf

To compile agains ndnSIM 2.7 / ns-3.29 (the preferred version is ndnSIM 2.8 / ns-3.30.1 - latest at the time of writing):

    sed -i 's/libns3.30.1-/libns3-dev-/g' .waf-tools/ns3.py
    sed -i 's/m_scheduler.scheduleEvent/m_scheduler.schedule/g' extensions/ndvr.cpp
    sed -i 's/scheduler::EventId/EventId/g' extensions/ndvr.hpp
    ./waf clean
    PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH ./waf configure --debug

Running
=======

After follow the steps for building the code, you can run NDVR as the following:

    NS_LOG=ndn.Ndvr ./waf --run ndn-ndvr-ring-3

The above command will run in verbose mode, showing lots of information from the 
protocol. To just run the code without debug:

    ./waf --run ndn-ndvr-ring-3

Emulated environment
====================

Using Docker:
	docker pull italovalcy/ndvr:latest
	docker run --rm -it --privileged -v /home/italo/ndvr:/ndvr -v /lib/modules:/lib/modules italovalcy/ndvr:latest bash -c 'python3 examples/ndvr-pingall.py --no-cli --nPings 60 /mini-ndn/topologies/default-topology.conf; grep -c content /tmp/minindn/*/ping-data/*.txt'


*Or* you can also build from source, following the steps below:

Install Mini-NDN:

	cd ~/
	git clone https://github.com/named-data/mini-ndn
	git checkout tags/v0.5.0
	./install.sh -a

Test if the MiniNDN installation succeeded:

	sudo rm -rf /tmp/minindn
	sudo python3 examples/nlsr/pingall.py --no-cli --nPings 60
	grep -c content /tmp/minindn/*/ping-data/*.txt

Install NDVR:

	cd ~/
	git clone https://github.com/italovalcy/ndvr
	cd ndvr
	git checkout ndvr-emu
	apt-get install protobuf-compiler libndn-cxx-dev libprotobuf-dev
	./waf configure --debug
	./waf
	sudo ./waf install

Copy the files needed to run NDVR under MiniNDN:

	cp config/validation.conf /usr/local/etc/ndn/ndvr-validation.conf
	cp minindn/apps/ndvr.py ~/mini-ndn/minindn/apps/ndvr.py
	cp minindn/ndvr* ~/mini-ndn/examples/
	cp minindn/nlsr-paper.py ~/mini-ndn/examples/nlsr/
	cp minindn/topologies/* ~/mini-ndn/topologies/

Test if NDVR integration with MiniNDN succeeded:

	sudo rm -rf /tmp/minindn
	sudo python examples/ndvr-pingall.py
	grep -c content /tmp/minindn/*/ping-data/*.txt

More information
================

For more information how to install NS-3 and ndnSIM, please refer to http://ndnsim.net website.

Note: this repository was based on https://github.com/named-data-ndnSIM/scenario-template.git 
