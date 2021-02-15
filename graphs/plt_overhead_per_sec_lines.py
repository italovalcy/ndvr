#!/usr/bin/python

import sys
import json
import numpy as np
import matplotlib.pyplot as plt

if len(sys.argv) < 1:
    print "USAGE: %s <data_file>"
    sys.exit(0)

data_file = sys.argv[1]

with open(data_file) as f:
    data = json.load(f)

sec = sorted(data.keys())
sec = sorted([int(k) for k in data.keys()])
helloint = []
dvinfoint = []
dvinfodat = []

for s in sec:
    s = str(s)
    helloint.append(np.mean(data[s]['ehlo_interest']))
    dvinfoint.append(np.mean(data[s]['dvinfo_interest']))
    dvinfodat.append(np.mean(data[s]['dvinfo_data']))

width=2.0
plt.figure(figsize=(20,5))
plt.plot(sec, helloint,  label = "DvAnnc_Interest", marker='.', linewidth=width)
plt.plot(sec, dvinfoint, label = "DvInfo_Interest", marker='x', linewidth=width)
plt.plot(sec, dvinfodat, label = "DvInfo_Data",     marker='v', linewidth=width)
plt.legend()
plt.show()
