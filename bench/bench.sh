#!/bin/env bash

DIR=$(dirname $0)

time $DIR/../build/corona $DIR/tcpd.js &
tcpdParentPid=$!
tcpdPid=$(ps -o pid,ppid | awk "{ if (\$2 == $tcpdParentPid) print \$1 }")

echo "Client ..."
time $DIR/../build/tcp -n 10000 -r 1000 -s 0 localhost 4000 || \
    echo "tcp exit status $?"

echo
echo "Server ..."
kill $tcpdPid
