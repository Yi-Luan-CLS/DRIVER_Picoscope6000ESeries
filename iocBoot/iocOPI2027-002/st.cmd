#!../../bin/deb12-x86_64/Picoscope

#- You may have to change Picoscope to something else
#- everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/Picoscope.dbd"
Picoscope_registerRecordDeviceDriver pdbbase

## Load record instances
dbLoadRecords("/home/luany/srv-unix-home/pico/PicoscopeApp/Db/Picoscope.template","user=luany")

cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=luany"
