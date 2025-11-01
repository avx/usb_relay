#!/bin/bash

for n in `seq 1 10`; do
    port=$(($RANDOM % 2 + 3))
    ./usb_relay -f /dev/ttyUSB0 -p $port -c switch
done
