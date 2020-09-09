#!/bin/bash

fgrep "ndn-cxx.nfd.Forwarder:onOutgoingInterest(): [DEBUG] onOutgoingInterest out=(257,0) interest=/localhop/ndvr/ehlo/" results/ndncomm2020.log > /tmp/ndvr-nfd-ehlo-interest
fgrep "ndn-cxx.nfd.Forwarder:onOutgoingInterest(): [DEBUG] onOutgoingInterest out=(257,0) interest=/localhop/ndvr/dvinfo/" results/ndncomm2020.log > /tmp/ndvr-nfd-dvinfo-interest
fgrep "ndn-cxx.nfd.Forwarder:onOutgoingData(): [DEBUG] onOutgoingData out=(257,0) data=/localhop/ndvr/dvinfo/" results/ndncomm2020.log > /tmp/ndvr-nfd-dvinfo-data

fgrep "nfd.Forwarder:onOutgoingInterest(): [DEBUG] onOutgoingInterest face=256 interest=/ndn/syncNotify/" ~/DDSN-Simulation/DDSN/result/1/raw/loss_rate_0.0-new.txt > /tmp/nfd-ddsn-sync-interest
fgrep "nfd.Forwarder:onOutgoingData(): [DEBUG] onOutgoingData face=256 data=/ndn/syncNotify/" ~/DDSN-Simulation/DDSN/result/1/raw/loss_rate_0.0-new.txt > /tmp/nfd-ddsn-sync-ack

echo "NDVR:"
wc -l /tmp/ndvr-nfd-*

echo "DDSN:"
wc -l /tmp/nfd-ddsn-*
