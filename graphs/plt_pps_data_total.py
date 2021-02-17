#!/usr/bin/python

import sys
import json
import numpy as np
from scipy.stats import sem, t
import matplotlib.pyplot as plt
import argparse

parser = argparse.ArgumentParser(description='Plot data')
parser.add_argument('--ndvr_mcast', metavar='FILE', required=True,
                    help='filename containing the data')
parser.add_argument('--ndvr_asf', metavar='FILE', required=True,
                    help='filename containing the data')
parser.add_argument('--asf', metavar='FILE', required=True,
                    help='filename containing the data')
parser.add_argument('--flood', metavar='FILE', required=True,
                    help='filename containing the data')
parser.add_argument('--seconds', type=int, required=True,
                    help='Number of seconds')

args = parser.parse_args()

strategies = ['ndvr_mcast', 'ndvr_asf', 'asf', 'flood']

data = {}
with open(args.ndvr_mcast) as f:
    data['ndvr_mcast'] = json.load(f)
with open(args.ndvr_asf) as f:
    data['ndvr_asf'] = json.load(f)
with open(args.asf) as f:
    data['asf'] = json.load(f)
with open(args.flood) as f:
    data['flood'] = json.load(f)


result_data = {}
intv_data = {}
result_total = {}
intv_total = {}
for s in strategies:
    result_data[s] = []
    intv_data[s] = []
    result_total[s] = []
    intv_total[s] = []

confidence = 0.95
for i in range(1, args.seconds):
    idx = "%d" % (i)
    for s in strategies:
        try:
            mydata = data[s][idx]['data']
            mean_data = np.mean(mydata)
            intv_data_v = sem(mydata) * t.ppf((1 + confidence) / 2, len(mydata) - 1)
        except:
            mean_data = 0
            intv_data_v = 0
        try:
            mydata = data[s][idx]['total']
            mean_total = np.mean(mydata)
            intv_total_v = sem(mydata) * t.ppf((1 + confidence) / 2, len(mydata) - 1)
        except:
            mean_total = 0
            intv_total_v = 0
        result_data[s].append(mean_data)
        intv_data[s].append(intv_data_v)
        result_total[s].append(mean_total)
        intv_total[s].append(intv_total_v)

width=2.0
#plt.figure(figsize=(20,10))
fig, axs = plt.subplots(2, figsize=(20,10))
axs[0].set_title("Data (pps)")
axs[1].set_title("Total (pps)")

for s in strategies:
    axs[0].plot(range(1, args.seconds), result_data[s],  label = s, linewidth=width)
    #axs[0].fill_between(range(1, args.seconds), np.array(result_data[s]) - np.array(intv_data[s]),  np.array(result_data[s]) + np.array(intv_data[s]))
    axs[1].plot(range(1, args.seconds), result_total[s],  label = s, linewidth=width)
fig.legend(loc = 'lower right')
plt.show()
