#!/usr/bin/python

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import sem, t
import argparse

parser = argparse.ArgumentParser(description='Process log file for SVS to calculate data retrieval delay')
parser.add_argument('--log', metavar='FILE', required=True,
                    help='filename containing the logs (NS_LOG=ndn.Svs)')
parser.add_argument('--num_nodes', type=int, required=True,
                    help='Number of nodes')
parser.add_argument('--debug', action="store_true", default=False, help='print sync time per node per nome')
parser.add_argument('--histogram', action="store_true", default=False, help='print histogram of sync time')
parser.add_argument('--output', metavar='FILE', help='Filename to save the results')

args = parser.parse_args()

class DataInfo:
    def __init__(self, adv_time):
        self.adv_time = adv_time
        self.count = 0
        self.last_seen = adv_time
    def __repr__(self):
        return "adv-time %s last-seen %s count %s" % (self.adv_time, self.last_seen, self.count)

data_store = {}
sync_duration = []
sync_duration_all = []
sync_dur_per_name_node = {}
counter_interest = 0
counter_data = 0
pps_i = {}
pps_d = {}
pps_g = {}

file = open(args.log)
for line in file:
    if line.find(" publishedData ") != -1:
        elements = line.split(' ')
        time = float(elements[0][1:-1])
        data_name = elements[-2].split('=')[1]
        data_store[data_name] = DataInfo(time)
        #print("publishdata %s" % data_name)
    elif line.find(" Received-data: ") != -1:
        elements = line.split(' ')
        time = float(elements[0][1:-1])
        data_name = elements[-2].split('=')[1]
        data_info = data_store[data_name]
        #print("retreivedata node=%s name=%s" % (elements[1],data_name))
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
        # goodput
        time_i = int(time)
        if time_i not in pps_g:
            pps_g[time_i] = 0
        pps_g[time_i] += 1
    elif line.find(" onOutgoingInterest out=(257,") != -1:
        counter_interest += 1
        elements = line.split(' ')
        time = int(float(elements[0][1:-1]))
        if time not in pps_i:
            pps_i[time] = 0
        pps_i[time] += 1
    elif line.find(" onOutgoingData out=(257,") != -1:
        counter_data += 1
        elements = line.split(' ')
        time = int(float(elements[0][1:-1]))
        if time not in pps_d:
            pps_d[time] = 0
        pps_d[time] += 1


total_data = 0
for k in sorted(data_store.keys()):
    #print("name=%s count=%s" % (k, data_store[k].count))
    total_data += data_store[k].count

#for sec in sorted(set(pps_i.keys() + pps_d.keys())):
#    print(sec,pps_i.get(sec, 0), pps_d.get(sec, 0))


data = sync_duration
confidence = 0.95
mean = np.mean(data)
intv = sem(data) * t.ppf((1 + confidence) / 2, len(data) - 1)
print "data sync duration = %f +- %f " % (mean, intv)
print "total data retreived = %d" % total_data
print "interest.count = %d" % counter_interest
print "data.count = %d" % counter_data

if args.output:
    import json
    with open(args.output, "w") as f:
        f.write(json.dumps({'duration': sync_duration, 'pps_i': pps_i, 'pps_d': pps_d, 'pps_g': pps_g}))

#if args.debug:
#    for w in sorted(sync_dur_per_name_node, key=sync_dur_per_name_node.get):
#        print(w, sync_dur_per_name_node[w])
#
#if args.histogram:
#    hist, bin_edges = np.histogram(sync_duration,bins=[0,1,2,3,4,5,10,15,20,40,60,80,100,120,140,160,180,200,300])
#    print(hist, bin_edges)
