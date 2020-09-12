#!/usr/bin/python

import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import sem, t
import matplotlib
import json
import argparse

parser = argparse.ArgumentParser(description='Plot data retrieve delay for DDSN and NDVR')
parser.add_argument('--ndvr', metavar='FILE', nargs='+', required=True,
                    help='filename containing the logs for NDVR')
parser.add_argument('--ddsn', metavar='FILE', nargs='+', required=True,
                    help='filename containing the logs for DDNS')
parser.add_argument('--tofile', metavar='FILE', type=str, nargs='?',
                    help='filename to save data')

args = parser.parse_args()


sum_ehlo_interest = []
sum_dvinfo_interest = []
sum_dvinfo_data = []
print "processing ndvr log files"
for filename in args.ndvr:
    f = open(filename)
    ehlo_interest = 0
    dvinfo_interest = 0
    dvinfo_data = 0
    for line in f:
        if line.find("ndn-cxx.nfd.Forwarder:onOutgoingInterest(): [DEBUG] onOutgoingInterest out=(257,0) interest=/localhop/ndvr/ehlo/") != -1:
            ehlo_interest += 1
        elif line.find("ndn-cxx.nfd.Forwarder:onOutgoingInterest(): [DEBUG] onOutgoingInterest out=(257,0) interest=/localhop/ndvr/dvinfo/") != -1:
            dvinfo_interest += 1
        elif line.find("ndn-cxx.nfd.Forwarder:onOutgoingData(): [DEBUG] onOutgoingData out=(257,0) data=/localhop/ndvr/dvinfo/") != -1:
            dvinfo_data += 1
    sum_ehlo_interest.append(ehlo_interest)
    sum_dvinfo_interest.append(dvinfo_interest)
    sum_dvinfo_data.append(dvinfo_data)

sum_sync_interest = []
sum_sync_ack = []
print "processing ddsn log files"
for filename in args.ddsn:
    f = open(filename)
    sync_interest = 0
    sync_ack = 0
    for line in f:
        if line.find("nfd.Forwarder:onOutgoingInterest(): [DEBUG] onOutgoingInterest face=256 interest=/ndn/syncNotify/") != -1:
            sync_interest += 1
        elif line.find("nfd.Forwarder:onOutgoingData(): [DEBUG] onOutgoingData face=256 data=/ndn/syncNotify/") != -1:
            sync_ack += 1
    sum_sync_interest.append(sync_interest)
    sum_sync_ack.append(sync_ack)

###############################
# Calc the mean and interval using 95% confidence level
print "calculating the mean and interval"
confidence = 0.95
ndvr_ehlo_mean = np.average(sum_ehlo_interest)
ndvr_ehlo_intv = sem(sum_ehlo_interest) * t.ppf((1 + confidence) / 2, len(sum_ehlo_interest) - 1)
ndvr_dvii_mean = np.average(sum_dvinfo_interest)
ndvr_dvii_intv = sem(sum_dvinfo_interest) * t.ppf((1 + confidence) / 2, len(sum_dvinfo_interest) - 1)
ndvr_dvid_mean = np.average(sum_dvinfo_data)
ndvr_dvid_intv = sem(sum_dvinfo_data) * t.ppf((1 + confidence) / 2, len(sum_dvinfo_data) - 1)

ddsn_syni_mean = np.average(sum_sync_interest)
ddsn_syni_intv = sem(sum_sync_interest) * t.ppf((1 + confidence) / 2, len(sum_sync_interest) - 1)
ddsn_syna_mean = np.average(sum_sync_ack)
ddsn_syna_intv = sem(sum_sync_ack) * t.ppf((1 + confidence) / 2, len(sum_sync_ack) - 1)

if args.tofile:
    data={}
    data['ndvr-ehlo-interest'] = [ndvr_ehlo_mean, ndvr_ehlo_intv]
    data['ndvr-dvinfo-interst'] = [ndvr_dvii_mean, ndvr_dvii_intv]
    data['ndvr-dvinfo-data'] = [ndvr_dvid_mean, ndvr_dvid_intv]
    data['ddsn-sync-interest'] = [ddsn_syni_mean, ddsn_syni_intv]
    data['ddsn-sync-ack'] = [ddsn_syna_mean, ddsn_syna_intv]
    with open(args.tofile, "w") as f:
        f.write(json.dumps(data))
else:
    print 'ndvr-ehlo-interest', ndvr_ehlo_mean, ndvr_ehlo_intv
    print 'ndvr-dvinfo-interst', ndvr_dvii_mean, ndvr_dvii_intv
    print 'ndvr-dvinfo-data', ndvr_dvid_mean, ndvr_dvid_intv
    print 'ddsn-sync-interest', ddsn_syni_mean, ddsn_syni_intv
    print 'ddsn-sync-ack', ddsn_syna_mean, ddsn_syna_intv

