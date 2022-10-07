# -*- Mode:python; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
#
# Copyright (C) 2015-2019, The University of Memphis,
#                          Arizona Board of Regents,
#                          Regents of the University of California.
#
# This file is part of Mini-NDN.
# See AUTHORS.md for a complete list of Mini-NDN authors and contributors.
#
# Mini-NDN is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Mini-NDN is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Mini-NDN, e.g., in COPYING.md file.
# If not, see <http://www.gnu.org/licenses/>.

import time
import random
from datetime import datetime

from mininet.log import lg, setLogLevel, info
import logging
from mininet.clean import sh
from minindn.minindn import Minindn
from minindn.util import MiniNDNCLI
from minindn.apps.app_manager import AppManager
from minindn.apps.nfd import Nfd
from minindn.apps.ndvr import Ndvr
from minindn.helpers.experiment import Experiment
from minindn.helpers.nfdc import Nfdc

import networkx as nx

import argparse

def myinfo(message):
    now = datetime.now()
    datefmt1 = now.strftime("%s.%f")
    datefmt2 = now.strftime("%Y-%m-%d,%H:%M:%S")
    info('{}/{} - {}'.format(datefmt1, datefmt2, message))

def mysleep(duration):
    myinfo('Sleep for {} seconds\n'.format(duration))
    time.sleep(duration)

def getParser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--interval', type=int, default=None,
                        help='NDVR Hello Interval')
    parser.add_argument('--no-cli', action='store_false', dest='isCliEnabled',
                        help='Run experiments and exit without showing the command line interface')
    parser.add_argument('--nPings', type=int, default=120,
                        help='Number of pings to perform between each node in the experiment')
    parser.add_argument('--ctime', type=int, default=60,
                        help='Specify convergence time for the topology (Default: 60 seconds)')
    parser.add_argument('--pct-traffic', dest='pctTraffic', type=float, default=1.0,
                        help='Specify the percentage of nodes each node should ping')
    parser.add_argument('--vbr', action='store_true', dest='enableVBR',
                        help='Run ping with Variable Bit Rate traffic model')
    parser.add_argument('--fw-strategy', type=str, default="best",
                        help='Specify the forwarding strategy (Default: best-route)')
    return parser

def mstFailure(ndn, nfds, ndvrs, args):
    mysleep(args.ctime)

    if args.enableVBR:
        options_pingserver = '--udist-size 400,1200'
        options_pingclient = '--poisson-interval 1000'
    else:
        options_pingserver = '--size 800'
        options_pingclient = None

    fw_strategy = Nfdc.STRATEGY_BEST_ROUTE
    if args.fw_strategy == 'asf':
        fw_strategy = Nfdc.STRATEGY_ASF

    myinfo('Starting ndnpingserver\n')
    Experiment.setupPing(ndn.net.hosts, fw_strategy, options=options_pingserver)
    myinfo('Starting ndnpingclient\n')
    pingedDict = Experiment.startPctPings(ndn.net, args.nPings, args.pctTraffic, options=options_pingclient, timeout=500)

    remaining_time = args.nPings

    mysleep(30)
    remaining_time -= 30
    elapsed_time = 30
    for host in ndn.net.hosts:
        host.cmd("nfdc fib list > /tmp/%s-fib-list-after-%d-sec.txt" % (host.name, elapsed_time))
        host.cmd("nfdc face list remote ether://[01:00:5e:00:17:aa] > /tmp/%s-face-list-after-%d-sec.txt" % (host.name, elapsed_time))

    data = open(args.topoFile).read()
    links = [line.split(" ")[0] for line in data.split("\n") if line.find("delay=") != -1]
    G = nx.Graph()
    for l in links:
        (a,b) = l.split(":")
        G.add_edge(a,b)
    mst = list(nx.minimum_spanning_edges(G, data=False))
    non_mst = list(set(list(G.edges())) - set(mst))
    random.shuffle(non_mst)
    #non_mst = [["MAR","TEL"],["BER","HAM"],["FRA","PRA"],["PAR","BRU"],["FRA","GEN"],["TIR","THE"],["IST","SOF"],["MIL","MAR"],["THE","SOF"],["ZAG","LUJ"],["BEL","ZAG"],["POR","LIS"],["LUX","FRA"],["VIE","MIL"],["BRA","VIE"],["SOF","SKO"],["DUB","PAR"],["VAR","BER"],["STO","HEL"],["NIC","ATH"],["BEL","POD"],["MAD","BIL"],["MAR","VAL"]]

    #mcn = max(ndn.net.hosts, key=lambda host: len(host.intfNames()))
    for link in non_mst:
        node_a = ndn.net.get(link[0].lower())
        node_b = ndn.net.get(link[1].lower())
        link_ab = ndn.net.linksBetween(node_a, node_b)[0]
        myinfo('Bringing down link {}<->{}\n'.format(link[0], link[1]))
        link_ab.intf1.node.cmd("ip link set down %s" % link_ab.intf1.name)
        link_ab.intf2.node.cmd("ip link set down %s" % link_ab.intf2.name)
        mysleep(2)
        elapsed_time += 2
        for host in ndn.net.hosts:
            host.cmd("nfdc fib list > /tmp/%s-fib-list-after-%d-sec.txt" % (host.name, elapsed_time))
            host.cmd("nfdc face list remote ether://[01:00:5e:00:17:aa] > /tmp/%s-face-list-after-%d-sec.txt" % (host.name, elapsed_time))
        mysleep(8)
        elapsed_time += 8
        remaining_time -= 10
        if remaining_time <= 30:
            break

        #mcn.intfs[i].link.intf1.config(loss=100.0)
        #mcn.intfs[i].link.intf2.config(loss=100.0)
        #mcn.cmd("ip link set down %s" % mcn.intfs[intf].name)
    #ndvrs[mcn.name].stop()
    #nfds[mcn.name].stop()

    #mysleep(30)

    #myinfo('Bringing up node {}\n'.format(mcn.name))
    #for i in mcn.intfs:
    #    mcn.intfs[i].link.intf1.config(loss=0.000001)
    #    mcn.intfs[i].link.intf2.config(loss=0.000001)
    #    #mcn.cmd("ip link set up %s" % mcn.intfs[intf].name)
    #nfds[mcn.name].start()
    #ndvrs[mcn.name].start()

    mysleep(remaining_time + 30)

if __name__ == '__main__':
    setLogLevel('info')
    ndn = Minindn(parser=getParser())
    args = ndn.args

    myinfo('Starting Minindn\n')
    ndn.start()

    myinfo('Starting nfd\n')
    nfds = AppManager(ndn, ndn.net.hosts, Nfd, logLevel='DEBUG')
    myinfo('Starting ndvr\n')
    ndvrs = AppManager(ndn, ndn.net.hosts, Ndvr, logLevel='ndvr.*=DEBUG', interval=args.interval) 

    myinfo('Starting mstFailure()\n')
    mstFailure(ndn, nfds, ndvrs, args)

    if args.isCliEnabled:
        MiniNDNCLI(ndn.net)

    myinfo('Stopping Minindn\n')
    ndn.stop()
