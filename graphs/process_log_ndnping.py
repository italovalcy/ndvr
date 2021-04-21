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

rec_pkt = {}
pkt_los = {}
pkt_rtt = {}

i=0
for filename in args.log:
    file = open(filename)
    for line in file:
        if line.find("content from") != -1:
            elements = line.split(' ')
            #mytime = elements[0].split('.')[0].split('T')[1]
            mytime = time.mktime(time.strptime(elements[0].split('.')[0], "%Y%m%dT%H%M%S"))
            if mytime not in rec_pkt:
                rec_pkt[mytime] = 0
                pkt_rtt[mytime] = []
            rec_pkt[mytime] += 1
            pkt_rtt[mytime].append(float(elements[-2].split('=')[1]))
        elif line.find("nack from") != -1 or line.find("timeout from") != -1:
            elements = line.split(' ')
            #mytime = elements[0].split('.')[0].split('T')[1]
            mytime = time.mktime(time.strptime(elements[0].split('.')[0], "%Y%m%dT%H%M%S"))
            if mytime not in pkt_los:
                pkt_los[mytime] = 0
            pkt_los[mytime] += 1
    i+=1

result = {}
total_rec = 0
total_los = 0
median_rtt = []
avg_rtt = []
for t in set(rec_pkt) | set(pkt_los):
    result[t] = {}
    result[t]['rec'] = rec_pkt.get(t, 0)
    result[t]['los'] = pkt_los.get(t, 0)
    total_rec += rec_pkt.get(t, 0)
    total_los += pkt_los.get(t, 0)
    if t in pkt_rtt:
        m = np.median(pkt_rtt[t])
        avg = np.average(pkt_rtt[t])
        result[t]['rtt_m'] = m
        result[t]['rtt_avg'] = avg
        median_rtt.append(m)
        avg_rtt.append(avg)

if args.debug:
    times = sorted(result)
    first_time = times[0]
    for t in times:
        print(t - first_time, time.strftime("%Y%m%dT%H%M%S", time.localtime(t)), result[t]['rec'], result[t]['los'], result[t].get('rtt_avg', ''))

if args.output:
    f = open(args.output, "w")
    f.write(json.dumps(result))

total = total_rec + total_los
rtt_avg = np.average(avg_rtt)   # mean or average
rtt_sd = np.std(avg_rtt)        # standard deviation
rtt_cv = 100.0*rtt_sd/rtt_avg   # coefficient of variation
print("SUMMARY: tx %d rx %d loss %d loss_pct %.4f rtt_median %.4f rtt_avg %.4f rtt_sd %.4f rtt_cd %.2f" % (total,total_rec,total_los,total_los*100.0/total, np.median(median_rtt), rtt_avg, rtt_sd, rtt_cv))
