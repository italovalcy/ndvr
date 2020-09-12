#!/usr/bin/python
import numpy as np
import matplotlib.pyplot as plt
import matplotlib
import json

# Generate data:
# NS_LOG=ndn.Ndvr ./waf --run "ndncomm2020-exp1 --wifiRange=60 --traceFile=trace/scenario-20.ns_movements" > results/ndncomm2020.log 2>&1
# egrep 'Store New Data: /ndn/vsyncData/%[0-9A-Fa-f]{2}/%0[1-5]/%00' ~/DDSN-Simulation/DDSN/result/1/raw/loss_rate_0.0.txt > /tmp/result
# --> Generate /tmp/data_ddsn
# ./graphs/process_log_ddsn.py /tmp/result 20
# --> Generate /tmp/data_ndvr
# ./graphs/process_log.py results/ndncomm2020.log 20

f = open('/tmp/data_ddsn', "r")
data1 = json.load(f)
sorted_data1 = np.sort(data1)
yvals1=np.arange(len(sorted_data1))/float(len(sorted_data1)-1)

f = open('/tmp/data_ndvr', "r")
data2 = json.load(f)
sorted_data2 = np.sort(data2)
yvals2=np.arange(len(sorted_data2))/float(len(sorted_data2)-1)

plt.legend()
plt.show()

font = {
    'family' : 'normal',
    'weight' : 'normal',
    'size'   : 14
}
matplotlib.rc('font', **font)

fig = plt.figure()
ax = fig.add_subplot(111)

ax.plot(sorted_data1, yvals1, label='DDSN', linestyle='-', linewidth=3)
ax.plot(sorted_data2, yvals2, label='NDVR', linestyle='-.', linewidth=3)


plt.ylim((0, 1))
plt.xlim((0, 1000))
plt.xlabel("Delay (s)")
plt.ylabel("CDF")
plt.legend(loc = 'center right', fancybox=True)
plt.grid(True)
plt.style.use('classic')
if save:
    fig.set_size_inches(6, 3)
    fig.savefig('tmp.pdf', format='pdf', dpi=1000)
else:
    plt.show()
