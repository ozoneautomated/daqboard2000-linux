ifndef PREFIX
PREFIX     = /usr/local
endif

SYSBIN_DIR = $(PREFIX)/bin
SYSLIB_DIR = $(PREFIX)/lib
SYSINC_DIR = $(PREFIX)/include


# Common flags
CC = gcc
DEBUG=-DCONFIG_DYNAMIC_DEBUG -DDEBUG_DB2K
#CFLAGS = -I$(INC_DIR) -L$(LIB_DIR) -fPIC -DDLINUX -Wall
CFLAGS = -ggdb $(DEBUG) -I$(INC_DIR) -L$(LIB_DIR) -fPIC -DDLINUX -Wall
#CFLAGS = -ggdb $(DEBUG) -I$(INC_DIR) -L$(LIB_DIR) -fPIC -DDLINUX -Wall -fno-stack-protector
#CFLAGS = -ggdb $(DEBUG) -I$(INC_DIR) -L$(LIB_DIR) -march=i486 -fPIC -DDLINUX -Wall

# Directories
INC_DIR  = include
LIB_SRC  = src
DRV_SRC  = db2kdriver
EXAM_SRC = examples

# Output directories
LIB_DIR  = ./lib
BIN_DIR  = ./bin
DRV_BASE = ./module

# targets
EXAMPLES = $(BIN_DIR)/bincvt $(BIN_DIR)/DaqAdcEx02\
	$(BIN_DIR)/DaqAdcEx03 $(BIN_DIR)/DaqAdcEx04\
	$(BIN_DIR)/DaqAdcEx05 $(BIN_DIR)/DaqAdcEx06\
	$(BIN_DIR)/DaqAdcEx07 $(BIN_DIR)/DaqAdcEx08\
	$(BIN_DIR)/DaqAdcEx10 $(BIN_DIR)/DaqAdcEx11\
	${BIN_DIR}/DaqAdcEx13 \
	$(BIN_DIR)/DaqDacEx01 $(BIN_DIR)/DaqDigIOEx01\
	$(BIN_DIR)/DaqDigIOEx02 $(BIN_DIR)/DaqTmrEx01\
	$(BIN_DIR)/DaqDacEx03 $(BIN_DIR)/DBK82Ex\
  

OBJS =  $(LIB_DIR)/apiadc.o $(LIB_DIR)/apical.o $(LIB_DIR)/apictr.o\
	$(LIB_DIR)/apictrl.o $(LIB_DIR)/apicvt.o $(LIB_DIR)/apidac.o\
	$(LIB_DIR)/apidbk.o $(LIB_DIR)/apidio.o $(LIB_DIR)/apiintr.o\
	$(LIB_DIR)/apiw32.o $(LIB_DIR)/cmn.o $(LIB_DIR)/linux_daqx.o\
	$(LIB_DIR)/w32ctrl.o $(LIB_DIR)/daqkeyhit.o

USER_HEADS = daqx daqx_linuxuser ddapi iottypes linux_daqx w32ioctl

LIB =   -ldaqx -lm
LIBNAME=$(LIB_DIR)/libdaqx.a
SHLIB = $(LIB_DIR)/libdaqx.so
VPATH = $(LIB_DIR):$(INC_DIR)




# Running kernel directory
KDIR ?= /lib/modules/`uname -r`/build
SYSDRV_DIR=/lib/modules/`uname -r`/kernel/drivers/daq/

all:	$(LIB_DIR) $(BIN_DIR) \
	driver $(LIBNAME) $(SHLIB) \
	$(EXAMPLES)

# create output directories
$(LIB_DIR):
	@if [ ! -d  $(LIB_DIR) ] ; then \
           echo "Making directory $(LIB_DIR)" ; \
           mkdir $(LIB_DIR); \
        fi;

$(BIN_DIR):
	@if [ ! -d  $(BIN_DIR) ] ; then \
           echo "Making directory $(BIN_DIR)" ; \
           mkdir $(BIN_DIR); \
        fi;

# Build the driver
# ADD V=1 for verbose build lines
# CFLAGS_MODULE is used for compiling the driver/module
KDEFS=-DCONFIG_DYNAMIC_DEBUG -DCONFIG_KALLSYMS -DCONFIG_HIGH_RES_TIMERS -DCONFIG_TRACEPOINTS
driver:
	$(MAKE) V=1  -C $(KDIR) M=$$PWD \
	CFLAGS_MODULE="$(KDEFS) -DDEBUG_DB2K -I/home/eol-lidar/rpmbuild/BUILD/lttng-modules-2.6.2"  modules


# DaqBoard2000 library
$(LIBNAME): $(OBJS)
	rm -f $@
	ar -crv $@ $^

$(SHLIB): $(OBJS)
	rm -f $@
	ld -shared -o $@ $^ $(LIBS) -lc

# library objects
#
$(LIB_DIR)/%.o:$(LIB_SRC)/%.c $(INC_DIR)/*.h $(LIB_SRC)/*.h
	$(CC) -D_DAQAPI32_=1 -c $(CFLAGS) $(OSFLAGS) -o $@ $<


# main binaries
#
$(BIN_DIR)/%:$(EXAM_SRC)/%.c
	$(CC) $(CFLAGS) $(OSFLAGS) -o $@ $< $(LIB) $(LIBS)

$(EXAMPLES): $(LIBNAME)



install: all install-driver install-lib

install-driver: driver 
# device driver
	@if [ -w /lib/modules/`uname -r`/kernel/ ] ; then\
	   echo "Installing driver to $(SYSDRV_DIR)" ; \
	   if [ ! -d  $(SYSDRV_DIR) ] ; then \
	      echo "Making directory $(SYSDRV_DIR)" ; \
	       mkdir -p $(SYSDRV_DIR); \
	   fi ;\
	   cp db2k.ko $(SYSDRV_DIR)/ && depmod;\
	else \
	   echo "No write access to install driver";\
	fi


install-lib:
# Copy header files
	@if [ -w `dirname $(SYSINC_DIR)` ] ; then\
	   echo "Installing include files to $(SYSINC_DIR)" ; \
	   if [ ! -d  $(SYSINC_DIR) ] ; then \
	      echo "Making directory $(SYSINC_DIR)" ; \
	      mkdir -p $(SYSINC_DIR); \
	   fi;\
	   for i in $(USER_HEADS) ; do \
	      echo $$i.h ; \
	      cp $(INC_DIR)/$$i.h $(SYSINC_DIR) ; \
	      chmod 644 $(SYSINC_DIR)/$$i.h ; \
	      done;\
	else\
	   echo "No write access to install includes: do it on the server.";\
	fi;

# Copy libdaqx.so and libdax.a and create /usr/lib symlinks
	@if [ -w `dirname $(SYSINC_DIR)` ] ; then\
	   echo "Installing library and to $(SYSLIB_DIR)";\
	   if [ ! -d  $(SYSLIB_DIR) ] ; then \
	      echo "Making directory $(SYSLIB_DIR)" ; \
	      mkdir -p $(SYSLIB_DIR); \
	   fi;\
	   for i in libdaqx.so libdaqx.a ; do \
	      echo $$i ; \
	      rm -f $(SYSLIB_DIR)/$$i ;\
	      cp $(LIB_DIR)/$$i $(SYSLIB_DIR) ; \
	      chmod 755 $(SYSLIB_DIR)/$$i ; \
		ln -s -f $(SYSLIB_DIR)/$$i /usr/lib/; \
	      ldconfig ;\
	   done;\
	else\
	   echo "No write access to install libs, running ldconfig only";\
	   ldconfig ;\
	fi;


uninstall:
	@for i in $(USER_HEADS) ; \
	  do \
	  echo $$i.h ; \
	  rm -f $(SYSINC_DIR)/$$i.h ; \
	  done

	@for i in libdaqx.so libdaqx.a ; \
	  do \
	  echo $$i ; \
	  rm -f $(SYSLIB_DIR)/$$i ;\
	  ldconfig ; \
	  done

	@if [ -d  $(SYSDRV_DIR) ] ; then \
	  echo "Removing directory $(SYSDRV_DIR)" ; \
	  rm -r $(SYSDRV_DIR);\
	  fi;

	rm -rf /dev/db2k/
	#delgroup db2k

	rm -f /usr/lib/libdaqx.so
	rm -f /usr/lib/libdaqx.a

load:
	./scripts/db2k_load

unload:
	rmmod db2k

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
	rm -rf $(BIN_DIR) $(LIB_DIR)
