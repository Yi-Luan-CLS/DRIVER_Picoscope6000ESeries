TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================

# Build the IOC application

PROD_IOC = Picoscope
# Picoscope.dbd will be created and installed
DBD += Picoscope.dbd

# Picoscope.dbd will be made up from these files:
Picoscope_DBD += base.dbd
Picoscope_DBD += Picoscope_support.dbd

# Include dbd files from all support applications:
#Picoscope_DBD += xxx.dbd

# Add all the support libraries needed by this IOC
# Picoscope_LIBS += /opt/picoscope/lib

# Picoscope_registerRecordDeviceDriver.cpp derives from Picoscope.dbd
Picoscope_SRCS += Picoscope_registerRecordDeviceDriver.cpp
Picoscope_SRCS += devPicoscope.c
Picoscope_SRCS += devPicoscopeMbbio.c
Picoscope_SRCS += devPicoscopeBio.c
Picoscope_SRCS += devPicoscopeStringio.c

Picoscope_SRCS += drvPicoscope.c
Picoscope_SRCS += devPicoscopeWaveform.c

# Build the main IOC entry point on workstation OSs.
Picoscope_SRCS_DEFAULT += PicoscopeMain.cpp
Picoscope_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#Picoscope_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
Picoscope_LIBS += $(EPICS_BASE_IOC_LIBS)
USR_SYS_LIBS += ps6000a
#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
SNCSEQ=$(TOP)/PicoscopeApp/src/picoscope
USR_CPPFLAGS += -Ofast -I$(SNCSEQ)/include/libps6000a
USR_LDFLAGS +=  -L$(SNCSEQ)/lib
USR_CPPFLAGS += -DUSE_TYPED_RSET 