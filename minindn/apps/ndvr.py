# -*- Mode:python; c-file-style:"gnu"; indent-tabs-mode:nil -*- */

import shutil
import os, sys, re

from mininet.clean import sh
from mininet.examples.cluster import RemoteMixin
from mininet.log import warn
from mininet.node import Switch

from minindn.apps.application import Application
from minindn.util import scp, copyExistentFile
from minindn.helpers.nfdc import Nfdc
from minindn.minindn import Minindn

class Ndvr(Application):
    BIN = '/usr/local/bin/ndvrd'

    def __init__(self, node, logLevel='NONE', network='/ndn', interval=None):
        Application.__init__(self, node)

        self.network = network
        self.interval = interval
        self.node = node
        self.parameters = self.node.params['params']
        self.prefixes = []

        if self.parameters.get('ndvr-log-level', None) != None:
            logLevel = self.parameters.get('ndvr-log-level')

        if logLevel in ['NONE', 'WARN', 'INFO', 'DEBUG', 'TRACE']:
            self.envDict = {'NDN_LOG': 'ndvr.*={}'.format(logLevel)}
        else:
            self.envDict = {'NDN_LOG': logLevel}

        if self.parameters.get('ndvr-prefixes', None) != None:
            self.prefixes = self.parameters.get('ndvr-prefixes').split(',')
        else:
            self.prefixes.append('/ndn/{}-site'.format(node.name))

        self.logFile = 'ndvr.log'
        self.routerName = '/{}C1.Router/{}'.format('%', node.name)
        self.validationConfFile = '{}/ndvr-validation.conf'.format(self.homeDir)

        possibleConfPaths = ['/usr/local/etc/ndn/ndvr-validation.conf', '/etc/ndn/ndvr-validation.conf']
        copyExistentFile(node, possibleConfPaths, self.validationConfFile)

        self.createKeysAndCertificates()

    def start(self):
        faces = self.listEthernetMulticastFaces()
        Application.start(self, '{} -n {} -r {} {} -v {} -f {} -p {}'.format(Ndvr.BIN 
                                    , self.network
                                    , self.routerName
                                    , '-i {}'.format(self.interval) if self.interval else '' 
                                    , self.validationConfFile
                                    , ' -f '.join(faces)
                                    , ' -p '.join(self.prefixes)
                                    ), 
                            self.logFile, 
                            self.envDict)
        Minindn.sleep(0.1)

    def listEthernetMulticastFaces(self):
        faces = []
        cmd = 'nfdc face list remote ether://[01:00:5e:00:17:aa]'
        output = self.node.cmd(cmd)
        for line in output.split("\n"):
            m = re.search('^faceid=([0-9]+)', line)
            if m:
                faces.append(m.group(1))
        return faces

    def createKeysAndCertificates(self):
        rootName = self.network
        rootCertFile = '{}/root.cert'.format(Minindn.workDir)

        # Create root certificate (only in the first run)
        if not os.path.isfile(rootCertFile):
            sh('ndnsec-keygen {}'.format(rootName)) # Installs a self-signed cert into the system
            sh('ndnsec-cert-dump -i {} > {}'.format(rootName, rootCertFile))

        # Create necessary certificates for each router
        shutil.copyfile(rootCertFile,
                        '{}/trust.cert'.format(self.homeDir))

        # Create router certificate
        routerName = '{}/%C1.Router/{}'.format(self.network, self.node.name)
        routerKeyFile = '{}/router.keys'.format(self.homeDir)
        routerCertFile = '{}/router.cert'.format(self.homeDir)
        self.node.cmd('ndnsec-keygen {} > {}'.format(routerName, routerKeyFile))

        # Copy routerKeyFile from remote for ndnsec-certgen
        if isinstance(self.node, RemoteMixin) and self.node.isRemote:
            login = 'mininet@{}'.format(self.node.server)
            src = '{}:{}'.format(login, routerKeyFile)
            dst = routerKeyFile
            scp(src, dst)

        # Root key is in root namespace, must sign router key and then install on host
        sh('ndnsec-certgen -s {} -r {} > {}'.format(rootName, routerKeyFile, routerCertFile))

        # Copy root.cert and site.cert from localhost to remote host
        if isinstance(self.node, RemoteMixin) and self.node.isRemote:
            login = 'mininet@{}'.format(self.node.server)
            src = '{}/router.cert'.format(self.homeDir)
            src2 = '{}/root.cert'.format(self.homeDir)
            dst = '{}:/tmp/'.format(login)
            scp(src, src2, dst)
            self.node.cmd('mv /tmp/*.cert {}'.format(self.homeDir))

        # finally, install the signed router certificate
        self.node.cmd('ndnsec-cert-install -f {}'.format(routerCertFile))
