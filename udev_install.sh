#!/bin/bash 

# Copy the udev rule file and reload all rules
cp PicoscopeApp/picoscopeSupport/95-pico.rules /etc/udev/rules.d 
udevadm control --reload-rule
