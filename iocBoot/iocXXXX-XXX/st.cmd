#!../../bin/deb12-x86_64/Picoscope

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/Picoscope.dbd"
Picoscope_registerRecordDeviceDriver pdbbase

##-----------------------------------------------------------------------------
## Device Configuration
##
## 1. Set your oscilloscope serial number and instance name below:
##    - SERIAL_NUM:  As printed on the back of your Picoscope (e.g., JR624/0023)
##    - OSC:         A short name prefix used for PVs (e.g., OSCJR624)
##
## 2. Replace all occurrences of OSC=OSCXXXX-XX and SERIAL_NUM=XXXX/XXXX
##    with your actual values.
##-----------------------------------------------------------------------------

dbLoadRecords("PicoscopeApp/Db/Picoscope.db", "OSC=OSCXXXX-XX, SERIAL_NUM=XXXX/XXXX")
dbLoadRecords("PicoscopeApp/Db/Picoscope.template", "OSC=OSCXXXX-XX, SERIAL_NUM=XXXX/XXXX, channel=A")
dbLoadRecords("PicoscopeApp/Db/Picoscope.template", "OSC=OSCXXXX-XX, SERIAL_NUM=XXXX/XXXX, channel=B")
dbLoadRecords("PicoscopeApp/Db/Picoscope.template", "OSC=OSCXXXX-XX, SERIAL_NUM=XXXX/XXXX, channel=C")
dbLoadRecords("PicoscopeApp/Db/Picoscope.template", "OSC=OSCXXXX-XX, SERIAL_NUM=XXXX/XXXX, channel=D")

##-----------------------------------------------------------------------------
## Initialize the Picoscope device
## Replace with actual serial number (must match what is used above)
##-----------------------------------------------------------------------------

PS6000ASetup("XXXX/XXXX")

cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=cid"
