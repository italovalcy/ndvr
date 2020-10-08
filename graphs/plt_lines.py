#!/usr/bin/python

import sys
import json
import numpy as np
import matplotlib.pyplot as plt

if len(sys.argv) < 3:
    print "USAGE: %s <global_best> <default_mul> <ndvr>"
    sys.exit(0)

global_best = sys.argv[1]
default_mul = sys.argv[2]
default_ran = sys.argv[3]
ndvr = sys.argv[4]

with open(global_best) as f:
    global_best_data = json.load(f)

with open(default_mul) as f:
    default_mul_data = json.load(f)

with open(default_ran) as f:
    default_ran_data = json.load(f)

with open(ndvr) as f:
    ndvr_data = json.load(f)

x1 = sorted([int(k) for k in global_best_data['retrieval_time_ps']])
y1 = [np.mean(global_best_data['retrieval_time_ps'][str(k)]) for k in x1]

x2 = sorted([int(k) for k in default_mul_data['retrieval_time_ps']])
y2 = [np.mean(default_mul_data['retrieval_time_ps'][str(k)]) for k in x2]

x3 = sorted([int(k) for k in ndvr_data['retrieval_time_ps']])
y3 = [np.mean(ndvr_data['retrieval_time_ps'][str(k)]) for k in x3]

x4 = sorted([int(k) for k in default_ran_data['retrieval_time_ps']])
y4 = [np.mean(default_ran_data['retrieval_time_ps'][str(k)]) for k in x3]

width=2.0
plt.plot(x1, y1, label = "Global+Best",       marker='.', linewidth=width)
plt.plot(x2, y2, label = "Default+Multicast", marker='x', linewidth=width)
plt.plot(x3, y3, label = "NDVR",              marker='v', linewidth=width)
plt.plot(x4, y4, label = "Default+Random",    marker='o', linewidth=width)
plt.legend()
plt.show()
