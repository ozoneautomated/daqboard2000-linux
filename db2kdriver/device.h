////////////////////////////////////////////////////////////////////////////
//
// device.h                        I    OOO                           h
//                                 I   O   O     t      eee    cccc   h
//                                 I  O     O  ttttt   e   e  c       h hhh
// -----------------------------   I  O     O    t     eeeee  c       hh   h
// copyright: (C) 2002 by IOtech   I   O   O     t     e      c       h    h
//    email: linux@iotech.com      I    OOO       tt    eee    cccc   h    h
//
////////////////////////////////////////////////////////////////////////////
//
//   Copyright (C) 86, 91, 1995-2002, Free Software Foundation, Inc.
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2, or (at your option)
//   any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software Foundation,
//   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
//  
////////////////////////////////////////////////////////////////////////////
//
// Thanks to Amish S. Dave for getting us started up!
// Savagely hacked by Paul Knowles <Paul.Knowles@unifr.ch>,  May, 2003
//
// Many Thanks Paul, MAH
//
////////////////////////////////////////////////////////////////////////////
//
// Please email liunx@iotech with any bug reports questions or comments
//
////////////////////////////////////////////////////////////////////////////
#ifndef DEVICE_H
#define DEVICE_H

#define ADC_MAXCHANS            16
#define REGIONS_REQUESTED 	((1 << 0))
#define REGS_REMAPPED 		((1 << 1))
#define IRQ_REQUESTED 		((1 << 2))
#define READBUF_ALLOCATED 	((1 << 3))
#define DMACHAIN0_ALLOCATED 	((1 << 4))
#define DMACHAIN1_ALLOCATED 	((1 << 5))
#define ADCTIMER_ADDED	 	((1 << 6))
#define DACTIMER_ADDED	 	((1 << 7))
#define WRITEBUF_ALLOCATED 	((1 << 8))
#define DMA_ADDRESS_PROBLEM 	((1 << 9))


typedef DWORD(*PCTNHANDLER) (DWORD, ...);
typedef DWORD *PMDL;
typedef DWORD KSPIN_LOCK;
typedef DWORD KIRQL;
typedef DWORD NTSTATUS;
typedef DWORD PHYSICAL_ADDRESS;

typedef struct {
	KSPIN_LOCK spinLock;
	KIRQL irql;
} MutexHandleT;

typedef BOOL TimeoutHandleT;
typedef BOOL AsynchEventHandleT;


#define exceeded2sec() ((time1.LowPart-time0.LowPart) > 20000000L) || ((time1.HighPart > time0.HighPart)&&(time1.LowPart>20000000L))
#define exceeded10ms() ((time1.LowPart-time0.LowPart) > 200000L) || ((time1.HighPart > time0.HighPart)&&(time1.LowPart>200000L))

#define DMAINPBUFSIZE  0x20000L
#define DMAOUTBUFSIZE  0x20000L

#define INPCHAINSIZE ((DMAINPBUFSIZE / 0x0800L) + 0x0004L)
#define OUTCHAINSIZE ((DMAOUTBUFSIZE / 0x0800L) + 0x0004L)

#define FPGAMAXSCANCOUNT 0x10000L

#define PCI9080_DMA0_MODE          0x00000080L
#define PCI9080_DMA0_PCI_ADDR      0x00000084L
#define PCI9080_DMA0_LOCAL_ADDR    0x00000088L
#define PCI9080_DMA0_COUNT         0x0000008CL
#define PCI9080_DMA0_DESC_PTR      0x00000090L
#define PCI9080_DMA0_CMD_STATUS    0x000000a8L
#define PCI9080_DMA0_THRESHOLD     0x000000b0L

#define PCI9080_DMA1_MODE          0x00000094L
#define PCI9080_DMA1_PCI_ADDR      0x00000098L
#define PCI9080_DMA1_LOCAL_ADDR    0x0000009CL
#define PCI9080_DMA1_COUNT         0x000000a0L
#define PCI9080_DMA1_DESC_PTR      0x000000a4L
#define PCI9080_DMA1_CMD_STATUS    0x000000a9L
#define PCI9080_DMA1_THRESHOLD     0x000000b2L
#define PCI9080_DMA_ARBIT          0x000000aCL

#define PLX_SYSTEM_IDS              0x908010B5
#define WBK22_SUBSYSTEM_IDS         0x205910B5

#define DAQBOARD200X_SYSTEM_IDS      0x04091616
#define DAQBOARD200X_SYSTEM_IDS_CPCI 0x04251616
#define DAQBOOK200X_SYSTEM_IDS       0x04341616

#define DAQBOARD2000_SUBSYSTEM_IDS  0x00021616
#define DAQBOARD2001_SUBSYSTEM_IDS  0x00041616
#define DAQBOARD2002_SUBSYSTEM_IDS  0x30001616
#define DAQBOARD2003_SUBSYSTEM_IDS  0xd0041616
#define DAQBOARD2004_SUBSYSTEM_IDS  0x10041616
#define DAQBOARD2005_SUBSYSTEM_IDS  0x20001616

//MAH: 04.02.04 New 1000 series
#define DAQBOARD1000_SUBSYSTEM_IDS  0x00021616
#define DAQBOARD1005_SUBSYSTEM_IDS  0x20001616

/*
 * This ``structure'' is the memory map of where things are for the 
 * local address side of things on the DaqBoard.  Don't change things 
 * about in here or you will be talking to the wrong registers on 
 * the DaqBoard.
 *
 * How this fits with the alignment chosen by the compiler is beyond 
 * me at the moment.  There should probably be __attribute__((packed))
 * here somewhere...?
 * In storage it really only need 256 bytes....
 * should ask for sizeof...
 */

typedef struct hw {
	volatile WORD acqControl;
	volatile WORD acqScanListFIFO;
	volatile DWORD acqPacerClockDivLow;

	volatile WORD acqScanCounter;
	volatile WORD acqPacerClockDivHigh;
	volatile WORD acqTriggerCount;
	volatile WORD fill2;
	volatile SHORT acqResultsFIFO;
	volatile WORD fill3;
	volatile SHORT acqResultsShadow;
	volatile WORD fill4;
	volatile SHORT acqAdcResult;
	volatile WORD fill5;
	volatile WORD dacScanCounter;
	volatile WORD fill6;

	volatile WORD dacControl;
	volatile WORD fill7;
	volatile SHORT dacFIFO;
	volatile WORD fill8[2];
	volatile WORD dacPacerClockDiv;
	volatile WORD refDacs;
	volatile WORD fill9;

	volatile WORD dioControl;
	volatile SHORT dioP3hsioData;
	volatile WORD dioP3Control;
	volatile WORD calEepromControl;
	volatile SHORT dacSetting[4];
	volatile SHORT dioP2ExpansionIO8Bit[32];

	volatile WORD ctrTmrControl;
	volatile WORD fill10[3];
	volatile SHORT ctrInput[4];
	volatile WORD fill11[8];
	volatile WORD timerDivisor[2];
	volatile WORD fill12[6];

	volatile WORD dmaControl;
	volatile WORD trigControl;
	volatile WORD fill13[2];
	volatile WORD calEeprom;
	volatile WORD acqDigitalMark;
	volatile WORD trigDacs;
	volatile WORD fill14;
	volatile SHORT dioP2ExpansionIO16Bit[32];
} HW, *HWptr;

#define acqStatus       acqControl
#define dacStatusReg    dacControl
#define dioStatus       dioControl
#define ctrTmrStatus    ctrTmrControl
#define dmaStatus       dmaControl
#define trigStatus      trigControl

#define dioP28255        dioP2ExpansionIO8Bit
#define dioP28255Control dioP2ExpansionIO8Bit[3]

#define AcqResetScanListFifo        0x0004
#define AcqResetResultsFifo         0x0002
#define AcqResetConfigPipe          0x0001

#define AcqResultsFIFOMore1Sample   0x0001
#define AcqResultsFIFOHasValidData  0x0002
#define AcqResultsFIFOOverrun       0x0004
#define AcqLogicScanning            0x0008
#define AcqConfigPipeFull           0x0010
#define AcqScanListFIFOEmpty        0x0020
#define AcqAdcNotReady              0x0040
#define ArbitrationFailure          0x0080
#define AcqPacerOverrun             0x0100
#define DacPacerOverrun             0x0200
#define AcqHardwareError            0x01c0

#define SeqStartScanList            0x0011
#define SeqStopScanList             0x0010

#define AdcPacerInternal            0x0030
#define AdcPacerExternal            0x0032
#define AdcPacerEnable              0x0031
#define AdcPacerEnableDacPacer      0x0034
#define AdcPacerDisable             0x0030

#define AdcPacerNormalMode          0x0060
#define AdcPacerCompatibilityMode   0x0061
#define AdcPacerInternalOutEnable   0x0008
#define AdcPacerExternalRising      0x0100

#define DacFull                     0x0001
#define RefBusy                     0x0002
#define TrgBusy                     0x0004
#define CalBusy                     0x0008
#define Dac0Busy                    0x0010
#define Dac1Busy                    0x0020
#define Dac2Busy                    0x0040
#define Dac3Busy                    0x0080

#define Dac0Enable                  0x0021
#define Dac1Enable                  0x0031
#define Dac2Enable                  0x0041
#define Dac3Enable                  0x0051
#define DacEnableBit                0x0001
#define Dac0Disable                 0x0020
#define Dac1Disable                 0x0030
#define Dac2Disable                 0x0040
#define Dac3Disable                 0x0050
#define DacResetFifo                0x0004
#define DacPatternDisable           0x0060
#define DacPatternEnable            0x0061
#define DacSelectSignedData         0x0002
#define DacSelectUnsignedData       0x0000

#define DacPacerInternal            0x0010
#define DacPacerExternal            0x0012
#define DacPacerEnable              0x0011
#define DacPacerDisable             0x0010
#define DacPacerUseAdc              0x0014
#define DacPacerInternalOutEnable   0x0008
#define DacPacerExternalRising      0x0100

#define CalEepromWriteEnable        0x0001
#define CalEepromWriteDisable       0x0000
#define CalEepromSendTwoBytes       0x0002
#define CalEepromSendOneByte        0x0000
#define CalEepromHoldLogic          0x0004
#define CalEepromReleaseLogic       0x0000

#define EepromWriteEnable           0x0006
#define EepromWriteDisable          0x0004
#define EepromReadStatusRegister    0x0500
#define EepromWriteStatusRegister   0x0100
#define EepromReadData              0x0003
#define EepromWriteData             0x0002

#define EepromDeviceNotReady        0x0001
#define EepromWriteEnabled          0x0002

#define AdcSelectSignedData         0x0051
#define AdcSelectUnsignedData       0x0050

#define TrigAnalog                  0x0000
#define TrigTTL                     0x0010
#define TrigTransHiLo               0x0004
#define TrigTransLoHi               0x0000
#define TrigAbove                   0x0000
#define TrigBelow                   0x0004
#define TrigLevelSense              0x0002
#define TrigEdgeSense               0x0000
#define TrigEnable                  0x0001
#define TrigDisable                 0x0000

#define ThresholdDacSelect          0x8000
#define HysteresisDacSelect         0x0000

#define PosRefDacSelect             0x0100
#define NegRefDacSelect             0x0000

#define TriggerEvent                0x0008

#define DmaCh0Enable                0x0001
#define DmaCh0Disable               0x0000
#define DmaCh1Enable                0x0011
#define DmaCh1Disable               0x0010

#define Timer0Disable               0x0000
#define Timer0Enable                0x0001

#define Timer1Disable               0x0010
#define Timer1Enable                0x0011

#define TimerAllDisable             0x0040
#define TimerAllEnable              0x0041

#define TimerDisable                0x0000
#define TimerEnable                 0x0001

#define Counter0CascadeDisable      0x0050
#define Counter0CascadeEnable       0x0051

#define Counter1CascadeDisable      0x0060
#define Counter1CascadeEnable       0x0061

#define Counter0Enable              0x0071
#define Counter1Enable              0x0081
#define Counter2Enable              0x0091
#define Counter3Enable              0x00a1
#define CounterAllEnable            0x00f1

#define CounterDisable              0x0000
#define CounterEnable               0x0001
#define CounterClear                0x0002
#define CounterClearOnRead          0x0004
#define CountRisingEdges            0x0008
#define CountFallingEdges           0x0000
#define CounterClearOnAcqStart      0x0100

#define p3HSIOIsInput               0x0050
#define p3HSIOIsOutput              0x0052

#define p2Is82c55                   0x0030
#define p2IsExpansionPort           0x0032

#ifdef ADCFILE

#define DIGIN        0x0001
#define COMP         0x0002
#define LASTCONFIG   0x0004
#define SSH          0x0008
#define SIGNED       0x0010

#define UNI          0x0080

const short cfgChGain[7] = {
	0x0000,
	0x0010,
	0x0018,
	0x0020,
	0x0028,
	0x0030,
	0x0038
};

const short cfgChNum[24][4] = {

  {0x0000, 0x0000, 0x0000, 0x0001,},
   {0x0000, 0x0000, 0x0040, 0x0001,},
  {0x0000, 0x0000, 0x0080, 0x0001,},
  {0x0000, 0x0000, 0x00c0, 0x0001,},
  {0x0000, 0x0000, 0x0000, 0x0002,},
  {0x0000, 0x0000, 0x0040, 0x0002,},
  
  {0x0000, 0x0000, 0x0080, 0x0002},
  {0x0000, 0x0000, 0x00c0, 0x0002,},
  {0x0000, 0x0000, 0x0000, 0x0005,},
  { 0x0000, 0x0000, 0x0040, 0x0005,},
  {   0x0000, 0x0000, 0x0080, 0x0005,},
  {   0x0000, 0x0000, 0x00c0, 0x0005,},
  {	0x0000, 0x0000, 0x0000, 0x0006,},
  {	0x0000, 0x0000, 0x0040, 0x0006,},
  { 0x0000, 0x0000, 0x0080, 0x0006,},
  {	0x0000, 0x0000, 0x00c0, 0x0006,},

  {	0x0000, 0x0000, 0x0000, 0x0041,},
  {	0x0000, 0x0000, 0x0040, 0x0041,},
  {	0x0000, 0x0000, 0x0080, 0x0041,},
  {	0x0000, 0x0000, 0x00c0, 0x0041,},
  {	0x0000, 0x0000, 0x0000, 0x0042,},
  { 0x0000, 0x0000, 0x0040, 0x0042,},
  {	0x0000, 0x0000, 0x0080, 0x0042,},
  {	0x0000, 0x0000, 0x00c0, 0x0042},
};

#if 0
const short cfgChristWhatIsThis[24][4] = {
	0x0000, 0x0000, 0x0000, 0x0001,
	0x0000, 0x0000, 0x0040, 0x0001,
	0x0000, 0x0000, 0x0080, 0x0001,
	0x0000, 0x0000, 0x00c0, 0x0001,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0040, 0x0000,
	0x0000, 0x0000, 0x0080, 0x0000,
	0x0000, 0x0000, 0x00c0, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0005,
	0x0000, 0x0000, 0x0040, 0x0005,
	0x0000, 0x0000, 0x0080, 0x0005,
	0x0000, 0x0000, 0x00c0, 0x0005,
	0x0000, 0x0000, 0x0000, 0x0004,
	0x0000, 0x0000, 0x0040, 0x0004,
	0x0000, 0x0000, 0x0080, 0x0004,
	0x0000, 0x0000, 0x00c0, 0x0004,
	0x0000, 0x0000, 0x0000, 0x0041,
	0x0000, 0x0000, 0x0040, 0x0041,
	0x0000, 0x0000, 0x0080, 0x0041,
	0x0000, 0x0000, 0x00c0, 0x0041,
	0x0000, 0x0000, 0x0000, 0x0040,
	0x0000, 0x0000, 0x0040, 0x0040,
	0x0000, 0x0000, 0x0080, 0x0040,
	0x0000, 0x0000, 0x00c0, 0x0040
};
#endif

#endif /* ADCFILE */

typedef struct _DMACHAIN {
	PDWORD pciAddr;
	PDWORD localAddr;
	ULONG transferByteCount;
	PDWORD descriptorPointer;
} DMACHAIN, *PDMACHAIN;

typedef struct {
	INT offset;
	DWORD gain;
} calibrationT;

typedef struct {
	DWORD preamble;
	calibrationT correctionDACSE[16][7][2];
	calibrationT correctionDACDE[8][7][2];
	calibrationT trigThresholdDAC[2];
	calibrationT trigHysteresisDAC[2];
	BYTE posRef;
	BYTE negRef;
	BYTE filler1[2];
	DWORD testTime;
	BYTE  date[11];
	BYTE time[9];
	DWORD postamble;
	BYTE filler2[3];
	BYTE checksum;
} eepromT;

#define AboveIndex   0
#define BelowIndex   1

/* PEK:FIXME
 * the following two structures are candidates for merging,
 * they duplicate info unnecessarly...
 * best bet would to tack the unique PCI_SITE stuff onto the end of
 * brdInfoT, since its present layout is used in readBoardInfo
 */
typedef struct pci_site {
	DWORD bus;
	DWORD slot;
	DWORD serialNumber;
	DWORD deviceType;
	DWORD protocol;
} PCI_SITE;

#define FILLER_SIZE 19
typedef struct {
	DWORD secretPassword;
	DWORD serialNumber;
	DWORD deviceType;
	BYTE filler[FILLER_SIZE];
	BYTE checksum;
} brdInfoT;

typedef enum {
	TRIGGER_EVENT = 0,
	STOP_EVENT,
	POSTSTOP_EVENT,
	WAITING_FOR_TRIGGER,
	WAITING_FOR_STOP
} EventT;

typedef struct {
	DWORD mode;
	WORD *buf;
	WORD *bufHead;
	PMDL bufMdl;
	DWORD bufSize;
	WORD enableBits;
} DacOutT;

typedef struct {

	DWORD finalDesc;
	ULONG finalDescCount;
	WORD numWaveforms;
	DWORD singleWaveformSize;
	DWORD padBlockStartIndex;
	PDWORD padBlockDescPointer;
	DWORD stopCount;
	DWORD stopCountInSamples;
	DWORD maxWaveformsPerBuf;
	DWORD dynamicWaveformBufSize;
	WORD *dynamicWaveformBufAddr;
	WORD *dynamicWaveformBufHead;
	BOOL dmaStopPending;
	BOOL dynamic;
} WFT;

typedef struct {
	TRIG_EVENT_T event;
	DWORD countValue;
	DWORD count;
	BOOL hystSatisfied;
	BOOL swEventOccurred;
	BOOL hwEventOccurred;
	WORD trigControl;
	DWORD eventScan;
} TrigT;

typedef struct {
	WORD opCode;
	WORD cascade;
	WORD clearOnRead;
	WORD edge;
	WORD enable;
} CtrT;

typedef struct {
	WORD opCode;
	WORD timerDivisor;
	WORD enable;
} TmrT;

typedef struct _DEVICE_EXTENSION {
	WORD *dmaInpBufHead;
	WORD *eventSearchPtr;
	DWORD lastScanCopied;
	DWORD scansAcquiredLastPass;
	DWORD scansToMoveToUserBuf;
	DWORD maxDmaInpBufScans;
	DWORD maxUserBufScans;

	DWORD scanSize;
	BOOL scanSizeOdd;
	BOOL scanConfiguredOK;
	DWORD scansInUserBuffer;
	DaqError currentAdcErrorCode;
	BOOL dmaInpActive;
	BOOL firstTime;

	TrigT trig[2];
	EventT eventExpected;
	DWORD adcLastFifoCheck;
	WORD adcTRUEOldCounterValue;
	DWORD adcScansInFifo;
	DWORD adcTRUECurrentScanCount;
	WORD adcOldCounterValue;
	DWORD adcLastScanCount;
	DWORD adcCurrentScanCount;
	DWORD adcClockSource;
	DWORD adcPacerClockDivisorHigh;
	DWORD adcPacerClockDivisorLow;
	WORD adcPacerClockSelection;
	WORD *userInpBuf;
	WORD *userInpBufHead;
	PMDL userInpBufMdl;
	DWORD userInpBufSize;
	DWORD userInpBufCyclic;
	DWORD adcStatusChgEvent;
	DWORD adcXferEvent;

	DWORD adcStatusFlags;
	BOOL adcXferThreadRunning;
	BOOL adcThreadDie;
	KSPIN_LOCK adcStatusSpinLock;
	KIRQL adcOldIrql;
	WORD *dmaOutBufHead;
	DWORD dmaOutBufSize;
	WORD pad[5];
	BOOL dmaOutActive;
	BOOL dacTriggerIsAdc;
	BOOL testingOutputDma;
	DaqError currentDacErrorCode;
	DWORD dacStopEvent;
	DWORD dacTrigSource;
	DWORD dacClockSource;
	WORD dacPacerClockDivisor;
	WORD dacPacerClockSelection;
	DacOutT dacOut[5];
	WFT wf;
	DWORD totalSamplesDmaed;
	DWORD totalSamplesRemoved;
	WORD dacOldCounterValue;
	DWORD dacLastScanCount;
	DWORD dacCurrentScanCount;
	DWORD dacXferEvent;
	DWORD dacStatusFlags;
	BOOL dacXferThreadRunning;
	BOOL dacThreadDie;

	KSPIN_LOCK dacStatusSpinLock;
	KIRQL dacOldIrql;

	BOOL usingAnalogInputs;
	BOOL usingAnalogOutputs;
	BOOL usingDigitalIO;
	BOOL usingCounterTimers;
	WORD numDacs;

	eepromT eeprom;
	brdInfoT boardInfo;
	BOOL calTableReadOk;
	TmrT timer[3];
	CtrT counter[5];
	WORD counterControlImage[4];
	DWORD p3HSIOConfig;
	WORD p3HSIOOutputShadow;
	WORD p28255OutputShadow[3];
	WORD p2ExpansionOutputShadow[32];
	DWORD busAndSlot;
	INTERFACE_TYPE hostBusType;
	ULONG hostBusNumber;
	BOOL hostBusValid;

	VOID *daqBaseAddress;
	VOID *plxVirtualAddress;

	AsynchEventHandleT adcEventHandle;
	TimeoutHandleT adcTimeoutHandle;

	AsynchEventHandleT dacEventHandle;
	TimeoutHandleT dacTimeoutHandle;

	BOOL online;
	VOID(*detach) (struct _DEVICE_EXTENSION * pde);

	volatile int cleanupflags;
	void *daq_base;
	void *plx_base;
	unsigned long daq_base_phys;
	unsigned long plx_base_phys;

	BOOL inAdcEvent;
	BOOL disableAdcEvent;
	spinlock_t adcStatusMutex;

	BOOL inDacEvent;
	BOOL disableDacEvent;
	spinlock_t dacStatusMutex;

	struct pci_dev *pdev;
	PCI_SITE psite;
	int irq;
	int id;
	/* needed for single process access tracking */
	spinlock_t open_lock;
	int user_count;

	DMACHAIN dmaInpChain[INPCHAINSIZE] __attribute__ ((aligned(16)));
	DMACHAIN dmaOutChain[OUTCHAINSIZE] __attribute__ ((aligned(16)));
	WORD *dmaInpBuf;
	WORD *dmaOutBuf;

	short *readbuf;
	dma_addr_t readbuf_busaddr;
	int readbufsize;
	short *writebuf;
	dma_addr_t writebuf_busaddr;
	int writebufsize;
	void *dma0_chain;
	void *dma1_chain;
	volatile int nints;
	struct timer_list adc_timer;
    struct timer_list dac_timer;

    WORD *dacModulebuffer;
    WORD *dacOutbuffer[5];
        void *in_buf;
        void *out_buf;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

typedef DEVICE_EXTENSION db2k_t;

typedef enum { TB10Mhz, TB5Mhz, TB1Mhz, TB100Khz } TimeBaseT;

extern DWORD entryCalcSamplesPerSecond(DWORD count, DWORD t2, DWORD t1);

extern NTSTATUS checkForEEPROM(PDEVICE_EXTENSION pde);
extern NTSTATUS loadFpga(PDEVICE_EXTENSION pde);
extern BOOL getBoardInfo(PDEVICE_EXTENSION pde);
extern BOOL readBoardInfo(PDEVICE_EXTENSION pde);

extern VOID disconnectIrq(VOID);
extern VOID dioRead(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID dioWrite(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID configureCtr(PDEVICE_EXTENSION pde, WORD ctrNo, WORD clearCtr);
extern VOID initializeCtrs(PDEVICE_EXTENSION pde);
extern VOID configureTmr(PDEVICE_EXTENSION pde, WORD tmrNo);
extern VOID initializeTmrs(PDEVICE_EXTENSION pde);
extern VOID ctrSetOption(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID ctrRead(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID ctrWrite(PDEVICE_EXTENSION pde, daqIOTP p);

extern BOOL initializeAdc(PDEVICE_EXTENSION pde);
extern BOOL uninitializeAdc(PDEVICE_EXTENSION pde);

extern DWORD adcTestInputDma(PDEVICE_EXTENSION pde, DaqAdcFlag chType);

extern VOID writeAcqScanListEntry(PDEVICE_EXTENSION pde, WORD scanListEntry);
extern WORD readAcqStatus(PDEVICE_EXTENSION pde);
extern VOID writeAcqControl(PDEVICE_EXTENSION pde, WORD control);
extern VOID writeAcqPacerClock(PDEVICE_EXTENSION pde, DWORD divisorHigh, DWORD divisorLow);
extern SHORT readAcqResultsFifo(PDEVICE_EXTENSION pde);
extern SHORT peekAcqResultsFifo(PDEVICE_EXTENSION pde);
extern DWORD readAcqScanCounter(PDEVICE_EXTENSION pde);
extern DWORD readAcqTriggerCount(PDEVICE_EXTENSION pde);
extern VOID writeAcqDigitalMark(PDEVICE_EXTENSION pde, WORD markValue);
extern WORD readAcqDigitalMark(PDEVICE_EXTENSION pde);

extern VOID writeDmaControl(PDEVICE_EXTENSION pde, WORD control);

extern WORD readDacStatus(PDEVICE_EXTENSION pde);
extern VOID writeDacControl(PDEVICE_EXTENSION pde, WORD control);
extern VOID writeDac(PDEVICE_EXTENSION pde, WORD dacNo, SHORT outValue);
extern VOID writeDacPacerClock(PDEVICE_EXTENSION pde, WORD divisor);
extern VOID writePosRefDac(PDEVICE_EXTENSION pde, BYTE posRef);
extern VOID writeNegRefDac(PDEVICE_EXTENSION pde, BYTE negRef);

extern VOID writeTrigControl(PDEVICE_EXTENSION pde, WORD trigCommand);
extern WORD readTrigStatus(PDEVICE_EXTENSION pde);
extern VOID writeTrigHysteresis(PDEVICE_EXTENSION pde, WORD trigHysteresis);
extern VOID writeTrigThreshold(PDEVICE_EXTENSION pde, WORD trigThreshold);

extern VOID writeCalEepromControl(PDEVICE_EXTENSION pde, WORD control);
extern WORD readCalEeprom(PDEVICE_EXTENSION pde);
extern VOID writeCalEeprom(PDEVICE_EXTENSION pde, WORD value);

extern WORD readCtrTmrStatus(PDEVICE_EXTENSION pde);
extern VOID writeTmrCtrControl(PDEVICE_EXTENSION pde, WORD control);
extern WORD readCtrInput(PDEVICE_EXTENSION pde, WORD ctrNo);
extern VOID writeTmrDivisor(PDEVICE_EXTENSION pde, WORD tmrNo, WORD divisor);
extern WORD readDioStatus(PDEVICE_EXTENSION pde);
extern VOID writeDioControl(PDEVICE_EXTENSION pde, WORD control);
extern WORD readHSIOPort(PDEVICE_EXTENSION pde);
extern VOID writeHSIOPort(PDEVICE_EXTENSION pde, WORD portValue);
extern WORD read8255Port(PDEVICE_EXTENSION pde, WORD portNumber);
extern VOID write8255Port(PDEVICE_EXTENSION pde, WORD portNumber, WORD portValue);
extern WORD readExpansionIOPort(PDEVICE_EXTENSION pde, WORD portNumber);
extern VOID writeExpansionIOPort(PDEVICE_EXTENSION pde, WORD portNumber, WORD portValue);

extern VOID adcConfigure(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID adcArm(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID adcDisarm(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID adcStart(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID adcStop(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID adcSoftTrig(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID adcGetStatus(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID waitForStatusChg(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID setOption(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID adcDevDspCmds(PDEVICE_EXTENSION pde, daqIOTP p);

extern VOID calConfigure(PDEVICE_EXTENSION pde, daqIOTP p);
extern BOOL readCalConstants(PDEVICE_EXTENSION pde, DaqCalTableTypeT tableType);
extern VOID activateReferenceDacs(PDEVICE_EXTENSION pde);

extern BOOL initializeDac(PDEVICE_EXTENSION pde);
extern BOOL uninitializeDac(PDEVICE_EXTENSION pde);

extern DWORD dacTestOutputDma(PDEVICE_EXTENSION pde, DaqAdcFlag chType);
extern BOOL dacGetAdcTriggerDacAvailable(PDEVICE_EXTENSION pde);
extern VOID dacWriteLocal(PDEVICE_EXTENSION pde, DWORD chan, WORD value);

extern VOID dacMode(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID dacWriteMany(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID dacConfigure(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID dacArm(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID dacDisarm(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID dacStart(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID dacStop(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID dacSoftTrig(PDEVICE_EXTENSION pde, daqIOTP p);
extern VOID dacGetStatus(PDEVICE_EXTENSION pde, daqIOTP p);

extern VOID adcGetHWInfo(PDEVICE_EXTENSION pde, daqIOTP p);

extern struct device *Daq_dev;  // for error reporting

#define  daqOnline()       TRUE

#endif
