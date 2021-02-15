#!/usr/bin/python
import numpy as np
import matplotlib.pyplot as plt
import matplotlib
import json
import argparse

parser = argparse.ArgumentParser(description='Plot histogram of satisfaction delay')
parser.add_argument('--data', metavar='FILE', nargs='+', required=False, default=[],
                    help='name of files containing data')
parser.add_argument('--outfile', metavar='FILE', type=str, nargs='?',
                    default='',
                    help='filename to save the plot')
args = parser.parse_args()

bins=np.array([0,1,2,3,4,5,10,15,20,40,60,80,100,120,140,160,180,200,300])
bincenters = 0.5*(bins[1:]+bins[:-1])
len_bins = len(bins) - 1

data = []
for i in range(len_bins):
    data.append([])

for filename in args.data:
    f = open(filename)
    d = json.load(f)
    hist, bin_edges = np.histogram(d,bins=bins)
    for i in range(len_bins):
        data[i].append(hist[i])

result_mean = []
result_std = []
for i in range(len_bins):
    result_mean.append(np.mean(data[i]))
    result_std.append(np.std(data[i]))


#print(result_mean,result_std,bincenters)
xlabels = [ "{:d}-{:d}".format(value, bins[idx+1]) for idx, value in enumerate(bins[:-1])]
plt.bar(range(len_bins), result_mean, width=0.5, color='r', yerr=result_std, tick_label=xlabels)
plt.xticks(rotation=45)
plt.xlabel("satisfaction delay (s)")
plt.ylabel("#sync")
plt.tight_layout()

#plt.hist(result_mean, bins = bins)
plt.show()
