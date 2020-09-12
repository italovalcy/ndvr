#!/bin/bash

MAX_RUN=12
#TRACE=trace/scenario-20_DDSN.ns_movements
TRACE=trace/scenario-20nodes-RPGM-500x500.ns_movements
SCENARIO=ndncomm2020-exp1

run_scenario() {
    local i=$1
    local RESULT_DIR=results/run-rpgm-$i
    rm -rf $RESULT_DIR/
    mkdir -p $RESULT_DIR
    local TIME=$(date +%s)
    echo "running $SCENARIO run=$i..."
    NS_LOG=ndn.Ndvr:ndn-cxx.nfd.Forwarder ./waf --run "$SCENARIO --wifiRange=60 --traceFile=$TRACE --syncDataRounds=10" > $RESULT_DIR/$SCENARIO.log 2>&1
    local TIME2=$(date +%s)
    echo "--> done run=$i duration=$((TIME2 - TIME))s"
}

./waf
for i in `seq 1 $MAX_RUN`; do
    run_scenario $i
    sleep 10
done
