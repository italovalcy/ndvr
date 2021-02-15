#!/usr/bin/python

import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import json
import argparse

parser = argparse.ArgumentParser(description='Plot overhead for DDSN and NDVR')
parser.add_argument('--data', metavar='FILE', type=str, required=True,
                    help='filename containing the processed data (use graphs/process_overhead-ndvr-vs-flood.py first)')
parser.add_argument('--outfile', metavar='FILE', type=str, nargs='?',
                    default='/tmp/output-plot.pdf',
                    help='filename to save the output when --save is used')
parser.add_argument('--save', dest='save', action='store_true',
                    help='save plot at <outfile> (default show)')

args = parser.parse_args()

with open(args.data) as f:
    data = json.load(f)

# NDVR, DDSN
ndvrInterest = (data['ndvr-interest'][0], 0)
ndvrInterest_std = (data['ndvr-interest'][1], 0)
ndvrData = (data['ndvr-data'][0], 0)
ndvrData_std = (data['ndvr-data'][1], 0)
floodInterest = (0, data['flood-interest'][0])
floodInterest_std = (0, data['flood-interest'][1])
floodData = (0, data['flood-data'][0])
floodData_std = (0, data['flood-data'][1])

font = {
    #'family' : 'normal',
    'family' : 'monospace',
    'weight' : 'normal',
    'size'   : 14
}
matplotlib.rc('font', **font)
        
fig = plt.figure()
ax = fig.add_subplot(111)

width = 0.35
ind = [1, 4]
labels = ['NDVR', 'Flood']

ax.bar(ind, ndvrInterest, width, yerr=ndvrInterest_std, hatch='/',  edgecolor='white', label='ndvr-interest')
ax.bar(ind, ndvrData, width, yerr=ndvrData_std, hatch='\\', edgecolor='white', label='ndvr-data', bottom=[i for i in ndvrInterest])
ax.bar(ind, floodInterest, width, yerr=floodInterest_std, hatch='.',  edgecolor='white', label='flood-interest')
ax.bar(ind, floodData, width, yerr=floodData_std, hatch='x',  edgecolor='white', label='flood-data', bottom=[i for i in floodInterest])

#plt.xlim((0, 5))
plt.xticks(ind, labels)
plt.ylabel("# Packets")
#plt.legend(loc = 'center right', fancybox=True)
plt.legend(loc = 'upper center', fancybox=True)
plt.grid(True, axis='y')
plt.style.use('classic')
fig.set_size_inches(8, 4)
if args.save:
    fig.savefig(args.outfile, format='pdf', dpi=1000)
else:
    plt.show()
