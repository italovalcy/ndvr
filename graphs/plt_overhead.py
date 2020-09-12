#!/usr/bin/python

import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import json
import argparse

parser = argparse.ArgumentParser(description='Plot overhead for DDSN and NDVR')
parser.add_argument('--data', metavar='FILE', type=str, required=True,
                    help='filename containing the processed data (use graph/process_overhead.py first)')
parser.add_argument('--outfile', metavar='FILE', type=str, nargs='?',
                    default='/tmp/output-plot.pdf',
                    help='filename to save the output when --save is used')
parser.add_argument('--save', dest='save', action='store_true',
                    help='save plot at <outfile> (default show)')

args = parser.parse_args()

with open(args.data) as f:
    data = json.load(f)

# NDVR, DDSN
ndvrEHLO = (data['ndvr-ehlo-interest'][0], 0)
ndvrEHLO_std = (data['ndvr-ehlo-interest'][1], 0)
ndvrDvII = (data['ndvr-dvinfo-interst'][0], 0)
ndvrDvII_std = (data['ndvr-dvinfo-interst'][1], 0)
ndvrDvID = (data['ndvr-dvinfo-data'][0], 0)
ndvrDvID_std = (data['ndvr-dvinfo-data'][1], 0)
ddsnSynI = (0, data['ddsn-sync-interest'][0])
ddsnSynI_std = (0, data['ddsn-sync-interest'][1])
ddsnSynA = (0, data['ddsn-sync-ack'][0])
ddsnSynA_std = (0, data['ddsn-sync-ack'][1])

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
labels = ['NDVR', 'DDSN']

ax.bar(ind, ndvrEHLO, width, yerr=ndvrEHLO_std, hatch='/',  edgecolor='white', label='ndvr-ehlo-interest')
ax.bar(ind, ndvrDvII, width, yerr=ndvrDvII_std, hatch='-',  edgecolor='white', label='ndvr-dvinfo-interest', bottom=[i for i in ndvrEHLO])
ax.bar(ind, ndvrDvID, width, yerr=ndvrDvID_std, hatch='\\', edgecolor='white', label='ndvr-dvinfo-data', bottom=[i+j for i,j in zip(ndvrEHLO, ndvrDvII)])
ax.bar(ind, ddsnSynI, width, yerr=ddsnSynI_std, hatch='.',  edgecolor='white', label='ddsn-sync-interest')
ax.bar(ind, ddsnSynA, width, yerr=ddsnSynA_std, hatch='x',  edgecolor='white', label='ddsn-sync-ack', bottom=[i for i in ddsnSynI])

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
