#!/usr/bin/python

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
import argparse
import re

parser = argparse.ArgumentParser(description='Process log file for DDSN to calculate data retrieval delay')
parser.add_argument('--filename', metavar='FILE', required=True,
                    help='filename containing the logs (NS_LOG=nfd.Forwarder) for DDSN')
parser.add_argument('--output', metavar='FILE', help='Filename to save the results')
parser.add_argument('--show', dest='show', action='store_true', default=False,
                    help='show plot (default not show)')

args = parser.parse_args()

data = {}
retrieval_time = []
retrieval_time_ps = {}

overhead_i = {}
overhead_d = {}
overhead_i_cnt = 0
overhead_d_cnt = 0

file = open(args.filename)
for line in file:
    if line.find("ndn.Consumer:SendPacket(): [INFO ] > Interest for") != -1:
        elements = line.split(' ')
        time = float(elements[0][1:-1])
        name = elements[-1]
        if name not in data:
            data[name] = time
    elif line.find("ndn.Consumer:OnData(): [INFO ] < DATA") != -1:
        elements = line.split(' ')
        time = float(elements[0][1:-1])
        name = elements[-1]
        assert name in data
        delta = time - data[name]
        retrieval_time.append(delta)
        time_int = int(data[name])
        if time_int not in retrieval_time_ps:
            retrieval_time_ps[time_int] = []
        retrieval_time_ps[time_int].append(delta)
        del data[name]
    elif re.search('dn-cxx\.nfd\.Forwarder:onOutgoingInterest.* interest=/ndn/dataSync', line):
        elements = line.split(' ')
        time = int(float(elements[0][1:-1]))
        overhead_i[time] = overhead_i.get(time, 0) + 1
        overhead_i_cnt += 1
    elif re.search('dn-cxx\.nfd\.Forwarder:onOutgoingData.* data=/ndn/dataSync', line):
        elements = line.split(' ')
        time = int(float(elements[0][1:-1]))
        overhead_d[time] = overhead_d.get(time, 0) + 1
        overhead_d_cnt += 1


print "retrieval time (mean) = " + str(np.mean(retrieval_time))
print "unanswered data = %s" % (len(data))
for k in sorted(retrieval_time_ps):
    print k,np.mean(retrieval_time_ps[k]),len(retrieval_time_ps[k])
print "total interest = %s" % (overhead_i_cnt)
print "total data = %s" % (overhead_d_cnt)
if args.output:
    import json
    to_save = {}
    to_save['retrieval_time'] = retrieval_time
    to_save['retrieval_time_ps'] = retrieval_time_ps
    to_save['overhead_i_cnt'] = overhead_i_cnt
    to_save['overhead_d_cnt'] = overhead_d_cnt
    with open(args.output, "w") as f:
        f.write(json.dumps(to_save))

if args.show:
    x1 = sorted(retrieval_time_ps)
    y1 = [np.mean(retrieval_time_ps[k]) for k in x1]
    plt.plot(x1, y1, label = "line 1", linewidth=1.0)
    plt.show()
