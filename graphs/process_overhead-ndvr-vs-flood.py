#!/usr/bin/python

import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import sem, t
import matplotlib
import json
import argparse

parser = argparse.ArgumentParser(description='Plot data retrieve delay for DDSN and NDVR')
parser.add_argument('--ndvr', metavar='FILE', nargs='+', required=False, default=[],
                    help='filename containing the logs for NDVR')
parser.add_argument('--flood', metavar='FILE', nargs='+', required=False, default=[],
                    help='filename containing the logs for Flood')
parser.add_argument('--tofile', metavar='FILE', type=str, nargs='?',
                    help='filename to save data')

args = parser.parse_args()


sum_interest = []
sum_data = []
print "processing ndvr log files"
for filename in args.ndvr:
    f = open(filename)
    interest = 0
    data = 0
    for line in f:
        if line.find("ndn-cxx.nfd.Forwarder:onOutgoingInterest(): [DEBUG] onOutgoingInterest out=(257,0) interest=/") != -1:
            interest += 1
        elif line.find("ndn-cxx.nfd.Forwarder:onOutgoingData(): [DEBUG] onOutgoingData out=(257,0) data=/") != -1:
            data += 1
    sum_interest.append(interest)
    sum_data.append(data)

sum_flood_interest = []
sum_flood_data = []
print "processing flood log files"
for filename in args.flood:
    f = open(filename)
    flood_interest = 0
    flood_data = 0
    for line in f:
        if line.find("ndn-cxx.nfd.Forwarder:onOutgoingInterest(): [DEBUG] onOutgoingInterest out=(257,0) interest=/") != -1:
            flood_interest += 1
        elif line.find("ndn-cxx.nfd.Forwarder:onOutgoingData(): [DEBUG] onOutgoingData out=(257,0) data=/") != -1:
            flood_data += 1
    sum_flood_interest.append(flood_interest)
    sum_flood_data.append(flood_data)

###############################
# Calc the mean and interval using 95% confidence level
print "calculating the mean and interval"
confidence = 0.95
ndvr_interest_mean = np.average(sum_interest)
ndvr_interest_intv = sem(sum_interest) * t.ppf((1 + confidence) / 2, len(sum_interest) - 1)
ndvr_data_mean = np.average(sum_data)
ndvr_data_intv = sem(sum_data) * t.ppf((1 + confidence) / 2, len(sum_data) - 1)

ddsn_floodi_mean = np.average(sum_flood_interest)
ddsn_floodi_intv = sem(sum_flood_interest) * t.ppf((1 + confidence) / 2, len(sum_flood_interest) - 1)
ddsn_floodd_mean = np.average(sum_flood_data)
ddsn_floodd_intv = sem(sum_flood_data) * t.ppf((1 + confidence) / 2, len(sum_flood_data) - 1)

if args.tofile:
    data={}
    data['ndvr-interest'] = [ndvr_interest_mean, ndvr_interest_intv]
    data['ndvr-data'] = [ndvr_data_mean, ndvr_data_intv]
    data['flood-interest'] = [ddsn_floodi_mean, ddsn_floodi_intv]
    data['flood-data'] = [ddsn_floodd_mean, ddsn_floodd_intv]
    with open(args.tofile, "w") as f:
        f.write(json.dumps(data))
else:
    print 'ndvr-interest', ndvr_interest_mean, ndvr_interest_intv
    print 'ndvr-data', ndvr_data_mean, ndvr_data_intv
    print 'flood-interest', ddsn_floodi_mean, ddsn_floodi_intv
    print 'flood-data', ddsn_floodd_mean, ddsn_floodd_intv
