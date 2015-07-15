#-----------------------
# device driver specific
#
DB2K_MAJOR = 61

obj-m	:= db2k.o

db2k-y := db2kdriver/adc.o
db2k-y += db2kdriver/cal.o
db2k-y += db2kdriver/ctr.o
db2k-y += db2kdriver/dac.o
db2k-y += db2kdriver/daqio.o
db2k-y += db2kdriver/dio.o
db2k-y += db2kdriver/linux_drvrint.o
db2k-y += db2kdriver/linux_entrypnt.o
db2k-y += db2kdriver/linux_init_utils.o

ccflags-y := -I$(src)/include -DDB2K_MAJOR=$(DB2K_MAJOR)



