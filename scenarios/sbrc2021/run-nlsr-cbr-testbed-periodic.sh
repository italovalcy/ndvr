#!/bin/bash

mkdir /tmp/minindn
sudo python examples/nlsr/nlsr-paper.py --onoff --sync chronosync --security --faces 1 --no-cli --nPings 180 --ctime 60 topologies/testbed.conf 2>&1 | tee /tmp/minindn/mndn.log
NOW=$(date +%Y%m%d%H%M%S)
echo "Finished at $NOW" | tee -a /tmp/minindn/mndn.log
mkdir -p /home/italo/results/nlsr-cbr-testbed-periodic
sudo mv /tmp/minindn /home/italo/results/nlsr-cbr-testbed-periodic/$NOW
sudo unlink /home/italo/results/nlsr-cbr-testbed-periodic/last
sudo ln -s /home/italo/results/nlsr-cbr-testbed-periodic/$NOW /home/italo/results/nlsr-cbr-testbed-periodic/last
