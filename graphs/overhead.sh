#!/bin/bash

usage() {
    echo "Usage: $0 <RUN> {ndvr|ddsn}"
    exit 0
}

RUN=$1
APP=$2

case "$APP" in
    ndvr)
        fgrep "ndn-cxx.nfd.Forwarder:onOutgoingInterest(): [DEBUG] onOutgoingInterest out=(257,0) interest=/localhop/ndvr/ehlo/"   results/run-$RUN/*.log > results/overhead-ndvr-ehlointers-$RUN
        fgrep "ndn-cxx.nfd.Forwarder:onOutgoingInterest(): [DEBUG] onOutgoingInterest out=(257,0) interest=/localhop/ndvr/dvinfo/" results/run-$RUN/*.log > results/overhead-ndvr-dvinfointe-$RUN
        fgrep "ndn-cxx.nfd.Forwarder:onOutgoingData(): [DEBUG] onOutgoingData out=(257,0) data=/localhop/ndvr/dvinfo/"             results/run-$RUN/*.log > results/overhead-ndvr-dvinfodata-$RUN
        echo "Overhead NDVR $RUN:"
        wc -l results/overhead-ndvr-*-$RUN
        ;;
    ddsn)
        fgrep "nfd.Forwarder:onOutgoingInterest(): [DEBUG] onOutgoingInterest face=256 interest=/ndn/syncNotify/" ~/DDSN-Simulation/DDSN/result/$RUN/raw/loss_rate_0.0-new.txt > results/overhead-ddsn-syncint-$RUN
        fgrep "nfd.Forwarder:onOutgoingData(): [DEBUG] onOutgoingData face=256 data=/ndn/syncNotify/"             ~/DDSN-Simulation/DDSN/result/$RUN/raw/loss_rate_0.0-new.txt > results/overhead-ddsn-syncack-$RUN
        echo "Overhead DDSN $RUN:"
        wc -l results/overhead-ddsn-*-$RUN
        ;;
    *)
        usage
        ;;
esac
