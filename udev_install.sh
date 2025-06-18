#!/bin/bash 

# Copy udev file 
cp PicoscopeApp/picoscopeSupport/95-pico.rules /etc/udev/rules.d 
udevadm control --reload-rule