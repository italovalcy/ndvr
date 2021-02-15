#!/usr/bin/python

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import sem, t
import argparse

parser = argparse.ArgumentParser(description='Process log file for NDVR to calculate data retrieval delay')
parser.add_argument('--file_name', metavar='FILE', required=True,
                    help='filename containing the logs (NS_LOG=ndn-cxx.nfd.Forwarder) for NDVR')
parser.add_argument('--num_nodes', type=int, required=True,
                    help='Number of nodes')
parser.add_argument('--debug', action="store_true", default=False, help='print sync time per node per nome')
parser.add_argument('--histogram', action="store_true", default=False, help='print histogram of sync time')
parser.add_argument('--output', metavar='FILE', required=True,
                    help='Filename to save the results')

args = parser.parse_args()

class DataInfo:
    def __init__(self, adv_time):
        self.adv_time = adv_time
        self.count = 0
        self.last_seen = adv_time

data_store = {}
sync_duration = []
sync_duration_all = []
sync_dur_per_name_node = {}

file = open(args.file_name)
for line in file:
    if line.find("ndn.Ndvr:AddNewNamePrefix()") != -1:
        elements = line.split(' ')
        time = float(elements[0][1:-1])
        data_name = elements[-1]
        data_store[data_name] = DataInfo(time)
    elif line.find("ndn.Ndvr:OnSyncDataContent()") != -1:
        elements = line.split(' ')
        time = float(elements[0][1:-1])
        data_name = elements[-1]
        data_info = data_store[data_name]
        data_info.count += 1
        data_info.last_seen = time
        # sanity check: the number of data sync cannot be greater than num_nodes-1
        assert data_info.count <= args.num_nodes - 1
        # individual sync duration
        sync_duration.append(data_info.last_seen - data_info.adv_time)
        sync_dur_per_name_node[elements[1] + '--' + data_name] = data_info.last_seen - data_info.adv_time
        # total sync duration (we care about 90% of the nodes, just as DDSN)
        if data_info.count == int(0.9*args.num_nodes):
            sync_duration_all.append(data_info.last_seen - data_info.adv_time)

data = sync_duration_all
confidence = 0.95
mean = np.mean(data)
intv = sem(data) * t.ppf((1 + confidence) / 2, len(data) - 1)
print "data sync duration = %f +- %f " % (mean, intv)

import json
with open(args.output, "w") as f:
    f.write(json.dumps(sync_duration))

if args.debug:
    for w in sorted(sync_dur_per_name_node, key=sync_dur_per_name_node.get):
        print(w, sync_dur_per_name_node[w])

if args.histogram:
    hist, bin_edges = np.histogram(sync_duration,bins=[0,1,2,3,4,5,10,15,20,40,60,80,100,120,140,160,180,200,300])
    print(hist, bin_edges)

#sorted_data = np.sort(sync_duration)
#yvals=np.arange(len(sorted_data))/float(len(sorted_data)-1)
#plt.plot(sorted_data,yvals)
#plt.show()
