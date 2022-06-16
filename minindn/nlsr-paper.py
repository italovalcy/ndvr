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

from mininet.log import lg, setLogLevel, info
import logging
from mininet.clean import sh
from minindn.minindn import Minindn
from minindn.util import MiniNDNCLI
from minindn.apps.app_manager import AppManager
from minindn.apps.nfd import Nfd
from minindn.apps.nlsr import Nlsr
from minindn.helpers.experiment import Experiment
from minindn.helpers.nfdc import Nfdc
from minindn.helpers.ndnpingclient import NDNPingClient

from nlsr_common import getParser

def mcnFailure(ndn, nfds, nlsrs, args):
    sh('dstat --epoch --cpu --mem > {}/dstat 2>&1 & echo $! > {}/dstat.pid'.format(args.workDir, args.workDir))
    sh('/usr/local/bin/get-cpu-usage.sh nlsr > {}/get-cpu-usage-nlsr 2>&1 & echo $! > {}/get-cpu-usage-nlsr.pid'.format(args.workDir, args.workDir))
    sh('/usr/local/bin/get-cpu-usage.sh ndnping > {}/get-cpu-usage-ndnping 2>&1 & echo $! > {}/get-cpu-usage-ndnping.pid'.format(args.workDir, args.workDir))
    sh('/usr/local/bin/get-cpu-usage.sh ndnpingserver > {}/get-cpu-usage-ndnpingserver 2>&1 & echo $! > {}/get-cpu-usage-ndnpingserver.pid'.format(args.workDir, args.workDir))
    sh('top -b -d 1 > {}/top 2>&1 & echo $! > {}/top.pid'.format(args.workDir, args.workDir))
    time.sleep(args.ctime)

    time.sleep(120)

    if args.nPings != 0:
        if args.enableVBR:
            Experiment.setupPing(ndn.net.hosts, Nfdc.STRATEGY_BEST_ROUTE, options='--udist-size 400,1200')
            pingedDict = Experiment.startPctPings(ndn.net, args.nPings, args.pctTraffic, options='--poisson-interval 1000')
        else:
            Experiment.setupPing(ndn.net.hosts, Nfdc.STRATEGY_BEST_ROUTE, options='--size 800')
            pingedDict = Experiment.startPctPings(ndn.net, args.nPings, args.pctTraffic)

    mcn = max(ndn.net.hosts, key=lambda host: len(host.intfNames()))
    if args.enableOnOff:
        nPings = args.nPings
        while nPings >= 60:
            time.sleep(30)
            info('Bringing down node {}\n'.format(mcn.name))
            nlsrs[mcn.name].stop()
            nfds[mcn.name].stop()
            time.sleep(30)
            info('Bringing up node {}\n'.format(mcn.name))
            nfds[mcn.name].start()
            nlsrs[mcn.name].start()
            nPings = nPings - 60

    else:
        time.sleep(60)

        info('Bringing down node {}\n'.format(mcn.name))
        nlsrs[mcn.name].stop()
        nfds[mcn.name].stop()

        time.sleep(60)

        info('Bringing up node {}\n'.format(mcn.name))
        nfds[mcn.name].start()
        nlsrs[mcn.name].start()
    time.sleep(60)
    sh('pkill -F {}/dstat.pid'.format(args.workDir))
    sh('pkill -F {}/get-cpu-usage-nlsr.pid'.format(args.workDir))
    sh('pkill -F {}/get-cpu-usage-ndnping.pid'.format(args.workDir))
    sh('pkill -F {}/get-cpu-usage-ndnpingserver.pid'.format(args.workDir))
    sh('pkill -F {}/top.pid'.format(args.workDir))


if __name__ == '__main__':
    setLogLevel('info')
    lg.ch.formatter = logging.Formatter('%(asctime)s - %(message)s')

    myparser = getParser()
    myparser.add_argument('--vbr', action='store_true', dest='enableVBR',
                        help='Run ping with Variable Bit Rate traffic model')
    myparser.add_argument('--onoff', action='store_true', dest='enableOnOff',
                        help='Enable On/Off failure model for the most connected node')

    ndn = Minindn(parser=myparser)
    args = ndn.args

    ndn.start()

    info('Starting nfd\n')
    nfds = AppManager(ndn, ndn.net.hosts, Nfd, logLevel='DEBUG')
    info('Starting nlsr\n')
    nlsrs = AppManager(ndn, ndn.net.hosts, Nlsr, sync=args.sync,
                       security=args.security, faceType=args.faceType,
                       nFaces=args.faces, routingType=args.routingType,
                       logLevel='ndn.*=DEBUG:nlsr.*=DEBUG')
                       #logLevel='NONE')

    info('Starting mcnFailure()\n')
    mcnFailure(ndn, nfds, nlsrs, args)

    if args.isCliEnabled:
        MiniNDNCLI(ndn.net)

    ndn.stop()
