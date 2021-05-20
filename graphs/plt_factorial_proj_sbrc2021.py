#!/usr/bin/python

import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import json
import argparse

parser = argparse.ArgumentParser(description='Plot overhead for DDSN and NDVR')
parser.add_argument('--data', metavar='FILE', type=str, required=True,
                    help='filename containing the processed data')
parser.add_argument('--output', metavar='FILE_PREFIX', type=str, nargs='?',
                    default='/tmp/output-plot', help='prefix filename to save the output')
parser.add_argument('--size', type=str, nargs='?', default='8x4', help='size in inches')

args = parser.parse_args()

inches = [int(v) for v in args.size.split('x')]

avg = {}
err = {}
groups = []
res_vars_names = None
for line in open(args.data):
    e = line.strip().split("\t")
    proto = e[1]
    factors = '{}-{}-{}'.format(e[2][:5], e[3][:5], e[4][:5])
    if res_vars_names is None:
        res_vars_names = e[5:]
        continue
    if factors not in groups:
        groups.append(factors)
    if proto not in avg:
        avg[proto] = {}
        err[proto] = {}
    i = 0
    for r in e[5:]:
        (val,ic) = [float(x) for x in r.split('+/-')]
        var = res_vars_names[i]
        if var not in avg[proto]:
            avg[proto][var] = []
            err[proto][var] = []
        avg[proto][var].append(val)
        err[proto][var].append(ic)
        i+=1

font = {
    #'family' : 'normal',
    'family' : 'monospace',
    'weight' : 'normal',
    'size'   : 14
}
matplotlib.rc('font', **font)

width = 0.35
x = np.arange(len(groups))

# https://matplotlib.org/stable/gallery/lines_bars_and_markers/barchart.html
i = 0
for var in res_vars_names:
    fig, ax = plt.subplots()
    fig.set_size_inches(inches[0], inches[1])
    ax.bar(x - width/2, avg['NDVR'][var], width, yerr=err['NDVR'][var], hatch='/', label='NDVR')
    ax.bar(x + width/2, avg['NLSR'][var], width, yerr=err['NLSR'][var], hatch='.', label='NLSR')
    #ax.set_title(var)
    ax.set_xticks(x)
    ax.set_xticklabels(groups, rotation=30, ha='right')
    ax.legend()
    fig.tight_layout()
    if args.output:
        fig.savefig('{}-{}.pdf'.format(args.output, i), format='pdf', dpi=1000)
    else:
        plt.show()
    i+=1
