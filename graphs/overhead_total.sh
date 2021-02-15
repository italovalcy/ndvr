#!/bin/bash

usage() {
    echo "Usage: $0 <LOG>"
    exit 0
}

if [ -z "$1" ]; then
    usage
fi

LOG=$1
NAME=$(basename $LOG)

fgrep "ndn-cxx.nfd.Forwarder:onOutgoingInterest(): [DEBUG] onOutgoingInterest out=(257,0) interest=/"  $LOG > results/log-interest-$NAME
fgrep "ndn-cxx.nfd.Forwarder:onOutgoingData(): [DEBUG] onOutgoingData out=(257,0) data=/"              $LOG > results/log-data-$NAME
echo "Overhead $LOG:"
wc -l results/log-*-$NAME
