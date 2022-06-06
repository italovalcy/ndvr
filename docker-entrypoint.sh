#!/bin/bash

alias python=python3

service openvswitch-switch start

exec "$@"
