#!/bin/bash

PROCESS=$1

if [ -z "$PROCESS" ]; then
	echo "USAGE: $0 NAME"
	exit 1
fi

while true; do pgrep -x $PROCESS | xargs -i cat /proc/{}/stat | awk -v ts=$(date +%s) '{user+=$14; kern+=$15; child+=$16;} END {print ts, user,kern,child}'; sleep 1; done
