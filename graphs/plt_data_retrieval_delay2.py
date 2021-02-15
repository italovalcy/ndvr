#!/usr/bin/python
import numpy as np
import matplotlib.pyplot as plt
import matplotlib
import json
import argparse

parser = argparse.ArgumentParser(description='Plot data retrieve delay for DDSN and NDVR')
parser.add_argument('--ndvr', metavar='FILE', nargs='+', required=False, default=[],
                    help='filename containing the logs for NDVR')
parser.add_argument('--ddsn', metavar='FILE', nargs='+', required=False, default=[],
                    help='filename containing the logs for DDNS')
parser.add_argument('--outfile', metavar='FILE', type=str, nargs='?',
                    default='',
                    help='filename to save the plot')
args = parser.parse_args()

# Generate data:
# NS_LOG=ndn.Ndvr ./waf --run "ndncomm2020-exp1 --wifiRange=60 --traceFile=trace/scenario-20.ns_movements" > results/ndncomm2020.log 2>&1
# egrep 'Store New Data: /ndn/vsyncData/%[0-9A-Fa-f]{2}/%0[1-5]/%00' ~/DDSN-Simulation/DDSN/result/1/raw/loss_rate_0.0.txt > /tmp/result
# --> Generate /tmp/data_ddsn
# ./graphs/process_log_ddsn.py /tmp/result 20
# --> Generate /tmp/data_ndvr
# ./graphs/process_log.py results/ndncomm2020.log 20

data1 = []
for filename in args.ddsn:
    f = open(filename)
    data1 += json.load(f)
#data1 = json.load(f)
sorted_data1 = np.sort(data1)
yvals1=np.arange(len(sorted_data1))/float(len(sorted_data1)-1)

#f = open('/tmp/data_ndvr', "r")
data2 = []
for filename in args.ndvr:
    f = open(filename)
    data2 += json.load(f)
sorted_data2 = np.sort(data2)
yvals2=np.arange(len(sorted_data2))/float(len(sorted_data2)-1)

#plt.legend()
#plt.show()

font = {
    'family' : 'monospace',
    'weight' : 'normal',
    'size'   : 14
}
matplotlib.rc('font', **font)

fig = plt.figure()
ax = fig.add_subplot(111)

#ax.plot(sorted_data1, yvals1, label='DDSN', linestyle='-', linewidth=3)
ax.plot(sorted_data1, yvals1, label='Flood', linestyle='-', linewidth=3)
ax.plot(sorted_data2, yvals2, label='NDVR', linestyle='-.', linewidth=3)


plt.ylim((0, 1))
plt.xlim((0, 200))
plt.xlabel("Delay (s)")
plt.ylabel("CDF")
plt.legend(loc = 'center right', fancybox=True)
plt.grid(True)
plt.style.use('classic')
if args.outfile:
    fig.set_size_inches(6, 3)
    fig.savefig(args.outfile, format='pdf', dpi=1000)
else:
    plt.show()
