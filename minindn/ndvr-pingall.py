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
import sys

from mininet.log import setLogLevel

from minindn.minindn import Minindn
from minindn.util import MiniNDNCLI
from minindn.apps.app_manager import AppManager
from minindn.apps.nfd import Nfd
from minindn.apps.ndvr import Ndvr
from minindn.helpers.experiment import Experiment
from minindn.helpers.nfdc import Nfdc

import argparse

DEFAULT_TOPO = '/home/minindn/mini-ndn/examples/topo-two-nodes.conf'

def getParser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--interval', type=int, default=None,
                        help='NDVR Hello Interval')
    parser.add_argument('--no-cli', action='store_false', dest='isCliEnabled',
                        help='Run experiments and exit without showing the command line interface')
    parser.add_argument('--nPings', type=int, default=300,
                        help='Number of pings to perform between each node in the experiment')
    return parser

if __name__ == '__main__':
    setLogLevel('info')
    ndn = Minindn(parser=getParser())
    args = ndn.args

    ndn.start()

    nfds = AppManager(ndn, ndn.net.hosts, Nfd, logLevel='DEBUG')
    ndvrs = AppManager(ndn, ndn.net.hosts, Ndvr, logLevel='ndvr.*=DEBUG', interval=args.interval) 

    if args.nPings != 0:
        Experiment.setupPing(ndn.net.hosts, Nfdc.STRATEGY_BEST_ROUTE)
        Experiment.startPctPings(ndn.net, args.nPings, 1.0)
        time.sleep(args.nPings + 10)

    if args.isCliEnabled:
        MiniNDNCLI(ndn.net)

    ndn.stop()
