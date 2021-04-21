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
parser.add_argument('--output', metavar='FILE', required=False,
                    help='Filename to save the results')
parser.add_argument('--debug', action="store_true", default=False, help='print more details')

args = parser.parse_args()

i=0
prev_time = {}
cur_time = {}
cur_pct = {}
for filename in args.log:
    file = open(filename)
    for line in file:
        if re.search("^top -", line):
            i+=1
            total_time = 0.0
            for k in cur_time:
                x = cur_time[k].split(":")
                y = prev_time.get(k, "0:00.0").split(":")
                total_time += (int(x[0]) - int(y[0]))*60 + (float(x[1]) - float(y[1]))
                prev_time[k] = cur_time[k]

            total_pct = 0.0
            for k in cur_pct:
                total_pct += float(cur_pct[k])
            print i, total_time, total_pct
        elif re.search("ndvrd$", line):
            elements = re.split("[ \t]+", line)
            cur_time[elements[0]] = elements[-2]
            cur_pct[elements[0]] = elements[-4]
#result = {}
#for t in set(rec_pkt) | set(pkt_los):
#    result[t] = {}
#    result[t]['rec'] = rec_pkt.get(t, 0)
#    result[t]['los'] = pkt_los.get(t, 0)
#
#times = sorted(result)
#first_time = times[0]
#for t in times:
#    print(t - first_time, result[t]['rec'], result[t]['los'])
#
#if args.output:
#    f = open(args.output, "w")
#    f.write(json.dumps(result))
