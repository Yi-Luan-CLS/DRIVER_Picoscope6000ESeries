#!../../bin/deb12-x86_64/Picoscope

#- You may have to change Picoscope to something else
#- everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/Picoscope.dbd"
Picoscope_registerRecordDeviceDriver pdbbase

## Load record instances

## OSC is the prefix for all PVs, it is expected to have the following format OSCXXXX-XX
## SERIAL_NUM is the serial number of the Picoscope to be opened by this application
dbLoadRecords("PicoscopeApp/Db/Picoscope.db", "OSC=OSC1022-11, SERIAL_NUM=JR624/0023")
dbLoadTemplate("PicoscopeApp/Db/Picoscope.substitutions", "OSC=OSC1022-11, SERIAL_NUM=JR624/0023")
PS6000ASetup("JR624/0023")

cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=cid"
