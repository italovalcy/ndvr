#!/usr/bin/python

import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import sem, t
import matplotlib
import json
import argparse
import re

parser = argparse.ArgumentParser(description='Process NDVR result file')
parser.add_argument('--ndvr', metavar='FILE', nargs='+', required=False, default=[],
                    help='filename containing the logs for NDVR')
parser.add_argument('--ddsn', metavar='FILE', nargs='+', required=False, default=[],
                    help='filename containing the logs for DDNS')
parser.add_argument('--tofile', metavar='FILE', type=str, nargs='?',
                    help='filename to save data')

args = parser.parse_args()


print "processing ndvr log files"
msgtype_per_sec = {}
i = 0
for filename in args.ndvr:
    f = open(filename)
    for line in f:
        if not re.match('^\+[0-9]+\.[0-9]+s ', line):
            continue
        elements = line.split(' ')
        time = int(float(elements[0][1:-1]))
        if time not in msgtype_per_sec:
            msgtype_per_sec[time] = {}
            msgtype_per_sec[time]['ehlo_interest'] = [0]*len(args.ndvr)
            msgtype_per_sec[time]['dvinfo_interest'] = [0]*len(args.ndvr)
            msgtype_per_sec[time]['dvinfo_data'] = [0]*len(args.ndvr)
        if line.find("ndn-cxx.nfd.Forwarder:onOutgoingInterest(): [DEBUG] onOutgoingInterest out=(257,0) interest=/localhop/ndvr/dvannc/") != -1:
            msgtype_per_sec[time]['ehlo_interest'][i] += 1
        elif line.find("ndn-cxx.nfd.Forwarder:onOutgoingInterest(): [DEBUG] onOutgoingInterest out=(257,0) interest=/localhop/ndvr/dvinfo/") != -1:
            msgtype_per_sec[time]['dvinfo_interest'][i] += 1
        elif line.find("ndn-cxx.nfd.Forwarder:onOutgoingData(): [DEBUG] onOutgoingData out=(257,0) data=/localhop/ndvr/dvinfo/") != -1:
            msgtype_per_sec[time]['dvinfo_data'][i] += 1

    i+=1

if args.tofile:
    f = open(args.tofile, "w")
    f.write(json.dumps(msgtype_per_sec))
    f.close()
else:
    for t in sorted(msgtype_per_sec.keys()):
        print("%d\t%.2f\t%.2f\t%.2f" % (t,np.mean(msgtype_per_sec[t]['ehlo_interest']), np.mean(msgtype_per_sec[t]['dvinfo_interest']), np.mean(msgtype_per_sec[t]['dvinfo_data'])))
