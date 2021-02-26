#!/usr/bin/python

import sys
import json
import numpy as np
from scipy.stats import sem, t
import matplotlib
import matplotlib.pyplot as plt
import argparse

parser = argparse.ArgumentParser(description='Plot data')
parser.add_argument('--ndvr_mcast', metavar='FILE', required=True,
                    help='filename containing the data')
parser.add_argument('--ndvr_masf', metavar='FILE', required=True,
                    help='filename containing the data')
parser.add_argument('--masf', metavar='FILE', required=True,
                    help='filename containing the data')
parser.add_argument('--flood', metavar='FILE', required=True,
                    help='filename containing the data')
parser.add_argument('--ndvr_ubest', metavar='FILE', required=True,
                    help='filename containing the data')
parser.add_argument('--seconds', type=int, required=True,
                    help='Number of seconds')
parser.add_argument('--aggregate', type=int, required=False, default=1,
                    help='aggregate seconds')
parser.add_argument('--output', metavar='FILE', type=str, nargs='?',
                    help='filename to save the output (otherwise it will just show)')

args = parser.parse_args()

strategies = ['ndvr_mcast', 'ndvr_masf', 'masf', 'mcast', 'ndvr_ubest']

data = {}
with open(args.ndvr_mcast) as f:
    data['ndvr_mcast'] = json.load(f)
with open(args.ndvr_masf) as f:
    data['ndvr_masf'] = json.load(f)
with open(args.masf) as f:
    data['masf'] = json.load(f)
with open(args.flood) as f:
    data['mcast'] = json.load(f)
with open(args.ndvr_ubest) as f:
    data['ndvr_ubest'] = json.load(f)

data_agg = {}
total_agg = {}
result_data = {}
intv_data = {}
result_total = {}
intv_total = {}
beans = args.seconds / args.aggregate
for s in strategies:
    data_agg[s] = [[]]*beans
    total_agg[s] = [[]]*beans
    result_data[s] = []
    intv_data[s] = []
    result_total[s] = []
    intv_total[s] = []

for i in range(1, args.seconds):
    b = i/args.aggregate
    idx = "%d" % i 
    for s in strategies:
        try:
            mydata = data[s][idx]['data']
        except:
            mydata = []
        data_agg[s][b] = np.append(data_agg[s][b], mydata)
        try:
            mydata = data[s][idx]['total']
        except:
            mydata = []
        total_agg[s][b] = np.append(total_agg[s][b], mydata)


confidence = 0.95
for i in range(0, args.seconds/args.aggregate):
    for s in strategies:
        mydata = data_agg[s][i]
        mean_data = np.mean(mydata)
        intv_data_v = sem(mydata) * t.ppf((1 + confidence) / 2, len(mydata) - 1)

        mydata = total_agg[s][i]
        mean_total = np.mean(mydata)
        intv_total_v = sem(mydata) * t.ppf((1 + confidence) / 2, len(mydata) - 1)

        result_data[s].append(mean_data)
        intv_data[s].append(intv_data_v)
        result_total[s].append(mean_total)
        intv_total[s].append( intv_total_v)

font = {
    #'family' : 'normal',
    'family' : 'monospace',
    'weight' : 'normal',
    'size'   : 20
}
matplotlib.rc('font', **font)

width=2.0
#plt.figurefigsize=(20,10))

fig, axs = plt.subplots(2, sharex=True, figsize=(20,10))
axs[0].set_title("Data delivery rate (all nodes)")
axs[1].set_title("Total forwarded packets (all nodes)")
axs[1].set_xlim([0, 95])

fig.add_subplot(111, frameon=False)
plt.xlabel("Time")
plt.ylabel("Packet per second")
plt.tick_params(labelcolor='none', top=False, bottom=False, left=False, right=False)

intervals = np.array(range(0, args.seconds/args.aggregate))*args.aggregate

styles = ['-', '--', '-.', ':', '-']

for s, ls in zip(strategies, styles):
    #axs[0].plot(intervals, result_data[s],  label = s, linewidth=width)
    axs[0].errorbar(intervals, result_data[s],  yerr=np.array(intv_data[s]), linestyle=ls, label = s, linewidth=width)
    #axs[0].fill_between(range(1, args.seconds), np.array(result_data[s]) - np.array(intv_data[s]),  np.array(result_data[s]) + np.array(intv_data[s]))
    #axs[1].plot(intervals, result_total[s],  label = s, linewidth=width)
    axs[1].errorbar(intervals, result_total[s],  yerr=np.array(intv_total[s]), linestyle=ls, label = s, linewidth=width)
    mydata = result_data[s]
    mean_data = np.mean(mydata)
    intv_data_v = sem(mydata) * t.ppf((1 + confidence) / 2, len(mydata) - 1)
    print(s,"data",mean_data,intv_data_v)
    mydata = result_total[s]
    mean_data = np.mean(mydata)
    intv_data_v = sem(mydata) * t.ppf((1 + confidence) / 2, len(mydata) - 1)
    print(s,"total", mean_data,intv_data_v)

fig.legend(loc = 'center right')

if args.output:
    fig.savefig(args.output, format='pdf', dpi=1000)
else:
    plt.show()
