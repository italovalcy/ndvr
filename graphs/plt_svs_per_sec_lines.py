#!/usr/bin/python

import sys
import json
import numpy as np
import matplotlib.pyplot as plt

if len(sys.argv) < 1:
    print "USAGE: %s <data_nortr> <data_ndvr>"
    sys.exit(0)

file_nortr = sys.argv[1]
file_ndvr = sys.argv[2]

with open(file_ndvr) as f:
    data_ndvr = json.load(f)

with open(file_nortr) as f:
    data_nortr = json.load(f)

sec = set(data_ndvr['pps_i'].keys() + data_ndvr['pps_d'].keys() + data_nortr['pps_i'].keys() + data_nortr['pps_d'].keys() + data_nortr['pps_g'].keys() + data_nortr['pps_g'].keys())
sec = sorted([int(s) for s in sec])
nortr_pps = []
nortr_pps_i = []
nortr_pps_d = []
nortr_pps_g = []
ndvr_pps = []
ndvr_pps_i = []
ndvr_pps_d = []
ndvr_pps_g = []

for s in sec:
    s = str(s)
    nortr_pps.append(data_nortr['pps_i'].get(s, 0)+data_nortr['pps_d'].get(s, 0))
    nortr_pps_i.append(data_nortr['pps_i'].get(s, 0))
    nortr_pps_d.append(data_nortr['pps_d'].get(s, 0))
    nortr_pps_g.append(data_nortr['pps_g'].get(s, 0))
    ndvr_pps.append(data_ndvr['pps_i'].get(s, 0)+data_ndvr['pps_d'].get(s, 0))
    ndvr_pps_i.append(data_ndvr['pps_i'].get(s, 0))
    ndvr_pps_d.append(data_ndvr['pps_d'].get(s, 0))
    ndvr_pps_g.append(data_ndvr['pps_g'].get(s, 0))

width=2.0
plt.figure(figsize=(20,5))
#plt.plot(sec, ndvr_pps,  label = "NDVR-total", marker='.', linewidth=width)
plt.plot(sec, ndvr_pps_g,  label = "NDVR-goodput", marker=',', linewidth=width)
#plt.plot(sec, nortr_pps, label = "NORT-total", marker='x', linewidth=width)
plt.plot(sec, nortr_pps_g, label = "NORT-goodput", marker='v', linewidth=width)
#plt.plot(sec, ndvr_pps_i,  label = "NDVR-I", marker='.', linewidth=width)
#plt.plot(sec, ndvr_pps_d,  label = "NDVR-D", marker=',', linewidth=width)
#plt.plot(sec, nortr_pps_i, label = "NORT-I", marker='x', linewidth=width)
#plt.plot(sec, nortr_pps_d, label = "NORT-D", marker='v', linewidth=width)
plt.legend()
plt.show()
