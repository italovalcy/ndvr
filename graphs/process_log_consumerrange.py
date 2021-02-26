#!/usr/bin/python

import os
import sys
import re
import json
import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import sem, t
import argparse

parser = argparse.ArgumentParser(description='Process log file to calculate data retrieval delay')
parser.add_argument('--log', metavar='FILE', required=True, nargs='+',
                    help='filename containing the logs (NS_LOG=ndn-cxx.nfd.Forwarder)')
parser.add_argument('--output', metavar='FILE', required=False,
                    help='Filename to save the results')
parser.add_argument('--debug', action="store_true", default=False, help='print more details')

args = parser.parse_args()

data_store = {}
total_store = {}

faces = {}
i=0
for filename in args.log:
    file = open(filename)
    for line in file:
        if line.find("added Face id") != -1:
            m = re.search('Node ([0-9]+): added Face id=([0-9]+) uri=.*', line)
            node = int(m.group(1))
            face = int(m.group(2))
            if node not in faces:
                faces[node] = {}
            faces[node][face] = {}
        if line.find("NEW-Neighbor_FaceId") != -1:
            elements = line.split(' ')
            node = int(elements[1])
            face = int(elements[-4])
            faces[node][face] = {}
        if line.find("ndn.RangeConsumer:OnSyncDataContent()") != -1:
            elements = line.split(' ')
            time = int(elements[0][1:-1].split('.')[0])
            node = int(elements[1])
            data_name = elements[-1]
            if time not in data_store:
                data_store[time] = {}
                data_store[time]['total'] = [0]*len(args.log)
            if node not in data_store[time]:
                data_store[time][node] = [0]*len(args.log)
            data_store[time][node][i] += 1
            data_store[time]['total'][i] += 1
        if re.search("ndn-cxx.nfd.Forwarder:onOutgoing(Data|Interest).* out=\(", line):
            elements = line.split(' ')
            m = re.search('out=\(([0-9]+),', elements[-2])
            face = int(m.group(1))
            time = int(elements[0][1:-1].split('.')[0])
            node = int(elements[1])
            if face not in faces[node]:
                #print(line)
                #print(faces[node])
                continue
            data_name = elements[-1]
            if time not in total_store:
                total_store[time] = {}
                total_store[time]['total'] = [0]*len(args.log)
            if node not in total_store[time]:
                total_store[time][node] = [0]*len(args.log)
            total_store[time][node][i] += 1
            total_store[time]['total'][i] += 1
    i+=1

result = {}
for time in set(sorted(data_store)) | set(sorted(total_store)):
    per_node = ""
    for node in sorted(data_store.get(time, {})):
        if node == 'total':
            continue
        per_node += " - %d:%s" % (node, data_store[time][node])
    try:
        total_data = data_store[time]['total']
    except:
        total_data = [0]
    try:
        total_total = total_store[time]['total']
    except:
        total_total = [0]
    if args.debug:
        print(time,total_data, per_node)
    else:
        print(time,total_data, total_total)
    result[time] = {}
    result[time]['data'] = total_data
    result[time]['total'] = total_total
    result[time]['data_per_node'] = data_store.get(time, {})
    result[time]['total_per_node'] = total_store.get(time, {})

if args.output:
    f = open(args.output, "w")
    f.write(json.dumps(result))
