#!/usr/bin/python

import os
import sys
import re
import json
import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import sem, t
import argparse
import time

parser = argparse.ArgumentParser(description='Process log file to parse multiple ndnping output')
parser.add_argument('--log', metavar='FILE', required=True, nargs='+',
                    help='filename containing the logs')

args = parser.parse_args()

data = []
for filename in args.log:
    file = open(filename)
    lines = file.readlines()
    if lines[-1].find("rtt min/avg/max/mdev") == -1:
        print('ERROR: file %s doesnt contains ndnping summary' % filename)
        continue
    data.append([float(x) for x in lines[-1].strip()[23:-3].split('/')])

a = np.array(data)
m = np.median(a, axis=0)
avg = np.average(a, axis=0)
# Latency CV - coefficient of variation (aka relative standard deviation - RSD) 
cv_m = 100.0*m[3] / m[1]
cv_avg = 100.0*avg[3]/avg[1]

print("median: min %.4f avg %.4f max %.4f mdev %.4f cv %.2f" % (m[0], m[1], m[2], m[3], cv_m))
print("average: min %.4f avg %.4f max %.4f mdev %.4f cv %.2f" % (avg[0], avg[1], avg[2], avg[3], cv_avg))

