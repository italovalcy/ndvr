#!/bin/bash

LOG=$1
NUM_NODES=$2

if [ -z "$LOG" -o -z "$NUM_NODES" ]; then
    echo "USAGE $0 <LOG> <NUM_NODES>"
    exit 0
fi

RESULT=$(for prefix in `egrep 'AddNewNamePrefix.*AdvName = /ndn/ndvrSync/' $LOG | awk '{print $NF}'`; do R=$(egrep "OnSyncDataContent.* $prefix" $LOG); echo $prefix --- num_sync=$(echo "$R" | wc -l); done)

if echo "$RESULT" | egrep -q -v "num_sync=$((NUM_NODES - 1))|num_sync=$NUM_NODES"; then
    echo "--> sync failed:"
    echo "$RESULT" | grep -v "num_sync=$((NUM_NODES - 1))"
else
    echo "--> all sync: $((NUM_NODES - 1))"
fi
