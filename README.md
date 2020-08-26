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

More information
================

For more information how to install NS-3 and ndnSIM, please refer to http://ndnsim.net website.

Note: this repository was based on https://github.com/named-data-ndnSIM/scenario-template.git 
