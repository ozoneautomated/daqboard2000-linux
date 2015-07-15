/* 
   w32ioctl.h : part of the DaqBoard2000 kernel driver 

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

#ifndef W32IOCTL_H
#define W32IOCTL_H

#ifdef __cplusplus
extern "C" {
#endif

#define IOTWRITE     0
#define IOTREAD      1

#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)

#define FUNCTION_BASE                  0x900

#define METHOD_BUFFERED                0
#define METHOD_IN_DIRECT               1
#define METHOD_OUT_DIRECT              2
#define METHOD_NEITHER                 3

#define FILE_ANY_ACCESS                0
#define FILE_READ_ACCESS               ( 0x0001 )
#define FILE_WRITE_ACCESS              ( 0x0002 )

#define IOTECH_TYPE                    0x5000

#define DOS_DEVICE_NAME     L"\\DosDevices\\daqbrd0"
#define NT_DEVICE_NAME      L"\\Device\\DAQBRD"

#define DB_REG_READ        CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + REG_READ        , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_REG_WRITE       CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + REG_WRITE       , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DDVER           CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DDVER           , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DIO_WRITE       CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DIO_WRITE       , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DIO_READ        CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DIO_READ        , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_ADC_CONFIG      CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + ADC_CONFIG      , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_ADC_START       CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + ADC_START       , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_ADC_STOP        CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + ADC_STOP        , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_ADC_ARM         CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + ADC_ARM         , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_ADC_DISARM      CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + ADC_DISARM      , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_ADC_SOFTTRIG    CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + ADC_SOFTTRIG    , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_ADC_STATUS      CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + ADC_STATUS      , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_ADC_STATUS_CHG  CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + ADC_STATUS_CHG  , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_ADC_HWINFO      CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + ADC_HWINFO      , METHOD_BUFFERED, FILE_ANY_ACCESS)

#define DB_CTR_WRITE       CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + CTR_WRITE       , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_CTR_READ        CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + CTR_READ        , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_CTR_XFER        CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + CTR_XFER        , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_CTR_STATUS      CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + CTR_STATUS      , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_CTR_START       CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + CTR_START       , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_CTR_STOP        CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + CTR_STOP        , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DAC_MODE        CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DAC_MODE        , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DAC_WRITE       CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DAC_WRITE       , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DAC_WRITE_MANY  CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DAC_WRITE_MANY  , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DAC_CONFIG      CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DAC_CONFIG      , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DAC_START       CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DAC_START       , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DAC_STOP        CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DAC_STOP        , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DAC_ARM         CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DAC_ARM         , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DAC_DISARM      CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DAC_DISARM      , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DAC_SOFTTRIG    CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DAC_SOFTTRIG    , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DAC_STATUS      CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DAC_STATUS      , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DAC_WTFIFO      CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DAC_WTFIFO      , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DAQ_TEST        CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DAQ_TEST        , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DAQ_ONLINE      CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DAQ_ONLINE      , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DAQPCC_OPEN     CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DAQPCC_OPEN     , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DAQPCC_CLOSE    CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DAQPCC_CLOSE    , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_CAL_CONFIG      CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + CAL_CONFIG      , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_ADC_CHAN_OPT    CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + ADC_CHAN_OPT    , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_DEV_DSP_CMDS    CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + DEV_DSP_CMDS    , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_SET_OPTION      CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + SET_OPTION      , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DB_BLOCK_MEM_IO    CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + BLOCK_MEM_IO    , METHOD_BUFFERED, FILE_ANY_ACCESS)

#ifdef __cplusplus
}
#endif
#endif
