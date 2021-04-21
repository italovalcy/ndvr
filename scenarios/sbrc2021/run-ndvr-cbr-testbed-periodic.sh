#!/bin/bash

mkdir /tmp/minindn
sudo python examples/ndvr-paper.py --onoff --no-cli --ctime 60 --nPings 180 topologies/testbed.conf 2>&1 | tee /tmp/minindn/mndn.log
NOW=$(date +%Y%m%d%H%M%S)
echo "Finished at $NOW" | tee -a /tmp/minindn/mndn.log
mkdir -p /home/italo/results/ndvr-cbr-testbed-periodic
sudo mv /tmp/minindn /home/italo/results/ndvr-cbr-testbed-periodic/$NOW
sudo unlink /home/italo/results/ndvr-cbr-testbed-periodic/last
sudo ln -s /home/italo/results/ndvr-cbr-testbed-periodic/$NOW /home/italo/results/ndvr-cbr-testbed-periodic/last
