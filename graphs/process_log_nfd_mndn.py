#!/usr/bin/python

import os
import sys
import re
import json
import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import sem, t
import argparse

parser = argparse.ArgumentParser(description='Process NFD log to calc overhead of messages (pkts/sec)')
parser.add_argument('--log', metavar='FILE', required=True, nargs='+',
                    help='filename containing the logs')
parser.add_argument('--output', metavar='FILE', required=False,
                    help='Filename to save the results')
parser.add_argument('--debug', action="store_true", default=False, help='print more details')

args = parser.parse_args()

proto = {'d':{}, 'i': {}}
app = {'d':{}, 'i': {}}

i=0
for filename in args.log:
    faces = ['-1'] # initializing with an invalid face to avoid wrong onOutgoing(Data|Interest) matches
    proto['d'][filename] = 0
    proto['i'][filename] = 0
    app['d'][filename] = 0
    app['i'][filename] = 0
    file = open(filename)
    for line in file:
        if line.find(" [nfd.FaceTable] Added face ") != -1:
            # Added face id=264 remote=udp4://10.0.0.125:6363 local=udp4://10.0.0.126:6363 
            m = re.search(' id=([0-9]+) remote=(ether|udp).*', line)
            if not m:
                continue
            faces.append(m.group(1))
        # 1616700170.828396 DEBUG: [nfd.Forwarder] onOutgoingData out=258 data=/localhost/nfd/faces/events/%FE%04
        #1616700170.828644 DEBUG: [nfd.Forwarder] onOutgoingInterest out=1 interest=/localhost/nfd/faces/events/%FE%05
        elif re.search(r" onOutgoing(Data|Interest) out=(%s)" % '|'.join(faces), line):
            name = line.split(' ')[-1]
            if name.find('/ping/') != -1:
                app[name[:1]][filename] += 1
            else:
                proto[name[:1]][filename] += 1
    i+=1

sum_app_i = np.sum(app['i'].values())
sum_app_d = np.sum(app['d'].values())
sum_proto_i = np.sum(proto['i'].values())
sum_proto_d = np.sum(proto['d'].values())
print("SUMMARY: sum_app_i %.2f sum_app_d %.2f sum_proto_i %.2f sum_proto_d %.2f" % (sum_app_i, sum_app_d, sum_proto_i, sum_proto_d))

if args.output:
    f = open(args.output, "w")
    f.write(json.dumps({'app': app, 'proto': proto}))
