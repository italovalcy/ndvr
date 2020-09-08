#!/usr/bin/python

import os
import sys
import numpy as np
import matplotlib.pyplot as plt

if len(sys.argv) < 3:
    print "USAGE %s <FILENAME> <NUM_NODES>" % (sys.argv[0])
    sys.exit(0)

class DataInfo:
    def __init__(self, adv_time):
        self.adv_time = adv_time
        self.count = 0
        self.last_seen = adv_time

file_name = sys.argv[1]
num_nodes = int(sys.argv[2])

data_store = {}
sync_duration = []
sync_duration_all = []

file = open(file_name)
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
        assert data_info.count <= num_nodes - 1
        # individual sync duration
        sync_duration.append(data_info.last_seen - data_info.adv_time)
        # total sync duration (we care about 90% of the nodes, just as DDSN)
        if data_info.count == int(0.9*num_nodes):
            sync_duration_all.append(data_info.last_seen - data_info.adv_time)

print "data sync duration (mean) = " + str(np.mean(sync_duration_all))
import json
with open('/tmp/data_ndvr', "w") as f:
    f.write(json.dumps(sync_duration))
#sorted_data = np.sort(sync_duration)
#yvals=np.arange(len(sorted_data))/float(len(sorted_data)-1)
#plt.plot(sorted_data,yvals)
#plt.show()
