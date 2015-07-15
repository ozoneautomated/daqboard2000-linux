/* 
   ddapi.h : part of the DaqBoard2000 kernel driver 

   Copyright (C) 86, 91, 1995-2002, Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
 */

/* Originally Written by 
  ////////////////////////////////////////////////////////////////////////////
  //                                 I    OOO                           h
  //                                 I   O   O     t      eee    cccc   h
  //                                 I  O     O  ttttt   e   e  c       h hhh
  // -----------------------------   I  O     O    t     eeeee  c       hh   h
  // copyright: (C) 2002 by IOtech   I   O   O     t     e      c       h    h
  //    email: linux@iotech.com      I    OOO       tt    eee    cccc   h    h
  ////////////////////////////////////////////////////////////////////////////

  Savagely hacked by Paul Knowles <Paul.Knowles@unifr.ch>,  May, 2003
 */
/*
 * stuff used only internally in the library and the driver
 * not to be explicitly included in user code...
 */

#ifndef DDAPI_H
#define DDAPI_H

#ifdef __cplusplus
extern "C" {
#endif
	typedef enum {
		REG_READ = 0x00,
		REG_WRITE = 0x01,
		DDVER = 0x02,
		DIO_WRITE = 0x03,
		DIO_READ = 0x04,
		ADC_CONFIG = 0x05,
		ADC_START = 0x06,
		ADC_STOP = 0x07,
		ADC_ARM = 0x08,
		ADC_DISARM = 0x09,
		ADC_SOFTTRIG = 0x0A,
		ADC_STATUS = 0x0B,
		CTR_WRITE = 0x0C,
		CTR_READ = 0x0D,
		CTR_XFER = 0x0E,
		CTR_STATUS = 0x0F,
		CTR_START = 0x10,
		CTR_STOP = 0x11,
		DAC_MODE = 0x12,
		DAC_WRITE = 0x14,
		DAC_WRITE_MANY = 0x15,
		DAC_CONFIG = 0x16,
		DAC_START = 0x17,
		DAC_STOP = 0x18,
		DAC_ARM = 0x19,
		DAC_DISARM = 0x1A,
		DAC_SOFTTRIG = 0x1B,
		DAC_STATUS = 0x1C,
		DAC_WTFIFO = 0x1D,
		DAQ_TEST = 0x1E,
		DAQ_ONLINE = 0x1F,
		ADC_STATUS_CHG = 0x20,
		ADC_HWINFO = 0x21,
		DAQPCC_OPEN = 0x22,
		DAQPCC_CLOSE = 0x23,
		CAL_CONFIG = 0x24,
		ADC_CHAN_OPT = 0x25,
		DEV_DSP_CMDS = 0x26,
		SET_OPTION = 0x27,
		BLOCK_MEM_IO = 0x28,
	} ItfCommandCodeT;


	typedef struct {
		DWORD errorCode;
		DWORD p1;
		DWORD p2;
		DWORD p3;
		DWORD p4;
		DWORD p5;
		DWORD p6;
		DWORD p7;
		DWORD p8;
		DWORD p9;
		DWORD p10;
		DWORD p11;
		DWORD p12;
		DWORD p13;
		DWORD p14;
		DWORD p15;
		DWORD p16;
		DWORD p17;
		DWORD p18;
		DWORD p19;
		DWORD p20;
		DWORD p21;
		DWORD p22;
		DWORD p23;
		DWORD p24;
		DWORD p25;
	} daqIOT, *daqIOTP;


	typedef struct {
		DWORD errorCode;
		char alias[80];
	} DAQPCC_OPEN_T, *DAQPCC_OPEN_PT;


	typedef enum {
		RegTypeDeviceRegs,
		RegTypeLptPort,
		RegTypeDspDM,
		RegTypeDspPM,
	} RegTypeT;


	typedef struct {
		DWORD errorCode;
		DWORD reg;
		DWORD value;
		DWORD regType;
	} REG_READ_T, *REG_READ_PT;


	typedef struct {
		DWORD errorCode;
		DWORD reg;
		DWORD value;
		DWORD regType;
	} REG_WRITE_T, *REG_WRITE_PT;


	typedef struct {
		DWORD errorCode;
		DWORD command;
		DWORD count;
		DWORD available;
		DWORD result;
	} DAQ_TEST_T, *DAQ_TEST_PT;


	typedef struct {
		DWORD errorCode;
		DWORD online;
	} DAQ_ONLINE_T, *DAQ_ONLINE_PT;


	typedef struct {
		DWORD errorCode;
		PCHAR idStr;
		DWORD idStrLen;
		DWORD idWritten;
		DWORD version;
	} DDVER_T, *DDVER_PT;


	typedef struct {
		DWORD errorCode;
		DWORD dioValue;
		DWORD dioDevType;
		DWORD dioPortOffset;
		DWORD dioWhichExpPort;
		DWORD dioBitMask;
	} DIO_WRITE_T, *DIO_WRITE_PT;


	typedef struct {
		DWORD errorCode;
		DWORD dioValue;
		DWORD dioDevType;
		DWORD dioPortOffset;
		DWORD dioWhichExpPort;
	} DIO_READ_T, *DIO_READ_PT;


	typedef enum { 
		DioTypeLocalBit, 
		DioTypeLocal, 
		DioTypeExp,
		DioMaxTypes
	} DioTypesT;


	typedef struct {
		DWORD channel;
		DWORD gain;
		DWORD flags;
		DWORD reserved;
	} SCAN_SEQ_T, *SCAN_SEQ_PT;


	typedef struct {
		DWORD TrigCount;
		DWORD TrigNumber;
		DWORD TrigEvent;
		DWORD TrigSource;
		DaqEnhTrigSensT TrigSens;
		DWORD TrigLevel;
		DWORD TrigVariance;
		DWORD TrigLocation;
		DWORD TrigChannel;
		DaqAdcGain TrigGain;
		DWORD Trigflags;
		DWORD reserved;
	} TRIG_EVENT_T, *TRIG_EVENT_PT;


#define RisingEdgeFlag 0x00010000
	typedef struct {
		DWORD errorCode;
		DWORD adcAcqMode;
		DWORD adcAcqPreCount;
		DWORD adcAcqPostCount;
		DWORD adcPeriodmsec;
		DWORD adcPeriodnsec;
		DWORD adcTrigSource;
		DWORD adcTrigLevel;
		DWORD adcTrigHyst;
		DWORD adcTrigChannel;
		SCAN_SEQ_PT adcScanSeq;
		DWORD adcScanLength;
		DWORD adcClockSource;
		DWORD adcPreTPeriodms;
		DWORD adcPreTPeriodns;
		PDWORD adcEnhTrigBipolar;
		DWORD adcEnhTrigCount;
		DaqAdcGain *adcEnhTrigGain;
		DWORD adcEnhTrigOpcode;
		PDWORD adcEnhTrigChan;
		DWORD adcDataPack;
		DaqEnhTrigSensT *adcEnhTrigSens;
		TRIG_EVENT_PT triggerEventList;
	} ADC_CONFIG_T, *ADC_CONFIG_PT;


	typedef struct {
		DWORD errorCode;
		DWORD adcXferBufLen;
		DWORD adcXferMode;
		PVOID adcXferBuf;
		DaqHandleT adcXferEvent;
	} ADC_START_T, *ADC_START_PT;


	typedef struct {
		DWORD errorCode;
		DWORD adcXferCount;
		DWORD adcAcqStatus;
		DWORD adcTriggerScan;
		DWORD adcPreTriggerCount;
		DWORD adcPostTriggerCount;
	} ADC_STATUS_T, *ADC_STATUS_PT;


	typedef struct {
		DWORD errorCode;
		DWORD adcAcqStatTail;
		DWORD adcAcqStatHead;
		DWORD adcAcqStatSamplesAvail;
		DWORD adcAcqStatScansAvail;
		DWORD adcAcqStatScansTotal;
	} ADC_ACQSTAT_T, *ADC_ACQSTAT_PT;


	typedef struct {
		DWORD errorCode;
		DWORD timeout;
		DWORD timedout;
	} ADC_STATUS_CHG_T, *ADC_STATUS_CHG_PT;


	typedef struct {
		DWORD errorCode;
		DWORD chan;
		DWORD option;
		DWORD usrPtr;
		DWORD dtEnum;
		DWORD chanType;
	} ADC_HWINFO_T, *ADC_HWINFO_PT;


	enum AdcHwInfo {
		AhiTime,
		AhiDate,
		AhiCycles,
		AhiChanType,
		AhiExtFeatures,
		AhiFifoSize,
		AhiFifoCount,
		AhiPostTrigScans,
		AhiFirmwareVer,
		AhiSerialNumber,
		AhiHardwareVer,
	};


	enum HwExtFeatures {
		AhefWbk30 = 0x00000001
	};


	typedef enum {
		DoptFifoOverflowMode = 1,
		DoptFifoCycleMode = 2,
		DoptFifoCycleSize = 3,
		DoptFifoFlush = 4,
		DoptFifoNoFlushOnDisarm = 5
	} DaqOption;


	typedef struct {
		DWORD errorCode;
		DaqOption doptType;
		DWORD dwValue;
	} SET_OPTION_T, *SET_OPTION_PT;


	typedef struct {
		DWORD errorCode;
		DWORD operation;
		DWORD startChan;
		DWORD endChan;
		DWORD devType;
		DWORD chanOptType;
		DWORD optValue;
	} ADC_CHAN_OPT_T, *ADC_CHAN_OPT_PT;


	typedef struct {
		DWORD errorCode;
		DWORD ctrDevType;
		DWORD ctrWhichExpPort;
		DWORD ctrPortOffset1;
		DWORD ctrValue1;
		DWORD ctrDoWrite1;
		DWORD ctrPortOffset2;
		DWORD ctrValue2;
		DWORD ctrBitMask;
	} CTR_WRITE_T, *CTR_WRITE_PT;


	typedef struct {
		DWORD errorCode;
		DWORD ctrDevType;
		DWORD ctrWhichExpPort;
		DWORD ctrPortOffset1;
		DWORD ctrValue1;
		DWORD ctrDoWrite1;
		DWORD ctrPortOffset2;
		DWORD ctrValue2;
	} CTR_READ_T, *CTR_READ_PT;


	typedef enum { 
		CtrLocal9513, 
		CtrExp9513, 
		CtrGeneric,
		CtrMaxTypes
	} CtrTypesT;


	typedef struct {
		DWORD errorCode;
		DWORD ctrNCounters;
		PVOID ctrBuffers;
		DWORD *ctrNSamples;
		BOOL ctrMode;
	} CTR_XFER_T, *CTR_XFER_PT;


	typedef struct {
		DWORD errorCode;
		DWORD ctrCurrentCount;
		DWORD ctrStatus;
		DWORD ctrChannel;
	} CTR_STATUS_T, *CTR_STATUS_PT;


	typedef struct {
		DWORD errorCode;
		DWORD ctrChannel;
	} CTR_START_T, *CTR_START_PT;


	typedef struct {
		DWORD errorCode;
		DWORD ctrChannel;
	} CTR_STOP_T, *CTR_STOP_PT;


	typedef struct {
		DWORD errorCode;
		DWORD dacDevType;
		DWORD dacChannel;
		DWORD dacOutputMode;
	} DAC_MODE_T, *DAC_MODE_PT;


	typedef struct {
		DWORD errorCode;
		DWORD dacDevType;
		DWORD dacChannel;
		PVOID dacValue;
	} DAC_WRITE_T, *DAC_WRITE_PT;


	typedef struct {
		DWORD errorCode;
		DWORD *dacDevTypes;
		DWORD *dacChannels;
		PVOID dacValues;
		DWORD dacNChannels;
	} DAC_WRITE_MANY_T, *DAC_WRITE_MANY_PT;


	typedef struct {
		DWORD errorCode;
		DWORD dacDevType;
		DWORD dacChannel;
		DWORD dacUpdateMode;
		DWORD dacUpdateCount;
		DWORD dacPeriodmsec;
		DWORD dacPeriodnsec;
		DWORD dacTrigSource;
		DWORD dacClockSource;
	} DAC_CONFIG_T, *DAC_CONFIG_PT;


	typedef struct {
		DWORD errorCode;
		DWORD dacDevType;
		DWORD dacChannel;
	} DAC_STOP_T, *DAC_STOP_PT, DAC_SOFTTRIG_T, *DAC_SOFTTRIG_PT;


	typedef struct {
		DWORD errorCode;
		DWORD dacDevType;
		DWORD dacChannel;
	} DAC_ARM_T, *DAC_ARM_PT, DAC_DISARM_T, *DAC_DISARM_PT;


	typedef struct {
		DWORD errorCode;
		DWORD dacDevType;
		DWORD dacChannel;
		DWORD dacXferBufLen;
		DWORD dacXferMode;
		PVOID dacXferBuf;
		DWORD dacXferEvent;
		DWORD dacXferBufHead;
		DWORD dacXferBufTail;
	} DAC_START_T, *DAC_START_PT;


	typedef struct {
		DWORD errorCode;
		DWORD dacDevType;
		DWORD dacChannel;
		DWORD dacCurrentCount;
		DWORD dacStatus;
		DWORD dacReadCount;
	} DAC_STATUS_T, *DAC_STATUS_PT;


	typedef enum {
		CoGetConstants,
		CoSetConstants,
		CoSelectInput,
		CoSelectTable,
		CoSaveConstants,
		CoGetCalPtr,
		CoSetResponseDac,
		CoSaveTable,
		CoGetRefDacConstants,
		CoSetRefDacConstants,
		CoGetTrigDacConstants,
		CoSetTrigDacConstants,
	} CalOperationT;


	typedef enum {
		CoptMainUnit,
		CoptOptionUnit,
		CoptUserCal,
		CoptSensorCal,
		CoptAuxCal1,
		CoptAuxCal2,
		CoptAuxCal3,
	} CalOptionT;


	typedef struct {
		DWORD errorCode;
		DWORD channel;
		DaqAdcGain gain;
		DaqAdcRangeT adcRange;
		DWORD gainConstant;
		INT offsetConstant;
		DaqCalTableTypeT tableType;
		DaqCalInputT calInput;
		CalOperationT operation;
		PDWORD calPtr;
		CalOptionT option;
	} CAL_CONFIG_T, *CAL_CONFIG_PT;


	typedef enum {
		BlockActionRead,
		BlockActionWrite,
		BlockActionInit,
	} BlockActionT;


	typedef enum {
		BlockTypeDspDM,
		BlockTypeDspPM,
		BlockTypeBootFlash,
		BlockTypeAdcFlash,
		BlockTypeMainEEPROM,
		BlockTypeOptionEEPROM,
	} BlockTypeT;


	typedef struct {
		DWORD errorCode;
		DWORD action;
		DWORD blockType;
		DWORD whichBlock;
		DWORD addr;
		DWORD count;
		DWORD buf;
	} BLOCK_MEM_IO_T, *BLOCK_MEM_IO_PT;


	typedef struct {
		DWORD errorCode;
		DWORD action;
		DWORD memType;
		DWORD addr;
		DWORD data;
	} DEV_DSP_CMDS_T, *DEV_DSP_CMDS_PT;

#ifdef __cplusplus
}
#endif
#endif
