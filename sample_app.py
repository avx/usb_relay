#!/usr/bin/python3
from usb_relay import usb_relay
import random

help(usb_relay)
r = usb_relay("/dev/ttyUSB0")

for port in range(1,5):
    print(r.status(port))

for i in range(3):
    port = random.randint(3,4)
    print(r.switch(port))

for port in range(1,5):
    print(r.status(port))
