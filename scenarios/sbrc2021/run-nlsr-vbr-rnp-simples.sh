#!/bin/bash

mkdir /tmp/minindn
sudo python examples/nlsr/nlsr-paper.py --vbr --sync chronosync --security --faces 1 --no-cli --nPings 180 --ctime 60 topologies/rnp.conf 2>&1 | tee /tmp/minindn/mndn.log
NOW=$(date +%Y%m%d%H%M%S)
echo "Finished at $NOW" | tee -a /tmp/minindn/mndn.log
mkdir -p /home/italo/results/nlsr-vbr-rnp-simples
sudo mv /tmp/minindn /home/italo/results/nlsr-vbr-rnp-simples/$NOW
sudo unlink /home/italo/results/nlsr-vbr-rnp-simples/last
sudo ln -s /home/italo/results/nlsr-vbr-rnp-simples/$NOW /home/italo/results/nlsr-vbr-rnp-simples/last
