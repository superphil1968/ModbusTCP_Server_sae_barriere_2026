/*
 * FreeModbus Libary: BARE Demo Application
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id: demo.c,v 1.1 2006/08/22 21:35:13 wolti Exp $

* modified from: Thanassis Mavrogeorgiadis 5-12-2011
 */

#include "mbed.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"
#include "mbconfig.h"


#if MB_TCP_ENABLED == 1
#include "EthernetNetIf.h"
EthernetNetIf eth;  
#endif

extern Serial pc;

DigitalOut led1(LED1);

DigitalIn ENET_LINK(P1_25);
DigitalOut LedLINK(LED4);
Ticker EtherPinMonitor;

#if MB_TCP_ENABLED == 1
void EtherPinMonitorFunc(void)
{
  LedLINK = !ENET_LINK; 
}
#endif

/* ----------------------- Defines ------------------------------------------*/
#define REG_INPUT_START 1001
#define REG_INPUT_NREGS 4

#define REG_HOLDING_START       2001
#define REG_HOLDING_NREGS       130

#define REG_COIL_START (3000+1)
#define REG_COIL_NREGS 8
#define REG_COIL_BYTES REG_COIL_NREGS/8

#define REG_DISC_START (4000+1)
#define REG_DISC_NREGS 8
#define REG_DISC_BYTES REG_DISC_NREGS/8

#define SLAVE_ID 0x0A

/* ----------------------- Static variables ---------------------------------*/
static USHORT   usRegInputStart = REG_INPUT_START;
static USHORT   usRegInputBuf[REG_INPUT_NREGS];
static USHORT   usRegHoldingStart = REG_HOLDING_START;
static USHORT   usRegHoldingBuf[REG_HOLDING_NREGS]={0x0123,0x4567,0x89AB,0xCDEF,0xDEAD,0xBEEF,0xDEAD,0xBEEF,0xDEAD,0xBEEF};
static USHORT    usRegCoilStart = REG_COIL_START;
static UCHAR     usRegCoilBuf[REG_COIL_BYTES]={0xA5};
static USHORT    usRegDiscStart = REG_DISC_START;
static UCHAR     usRegDiscBuf[REG_DISC_BYTES]={0x5A};

/* ----------------------- Start implementation -----------------------------*/


int main() {
  eMBErrorCode    eStatus;

#if MB_TCP_ENABLED == 1
  pc.baud(115200);
  EtherPinMonitor.attach(&EtherPinMonitorFunc, 0.01);
  printf("Setting up...\n");
  EthernetErr ethErr = eth.setup();
  if(ethErr)
  {
    printf("Error %d in setup.\n", ethErr);
    return -1;
  }
  printf("Setup OK\n");
  IpAddr ip = eth.getIp();
  printf("mbed IP Address is %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
#endif
   
  Timer tm;
  tm.start();

#if MB_RTU_ENABLED == 1
    eStatus = eMBInit( MB_RTU, SLAVE_ID, 0, 9600, MB_PAR_NONE );
#endif
#if MB_ASCII_ENABLED == 1
    eStatus = eMBInit( MB_ASCII, SLAVE_ID, 0, 9600, MB_PAR_NONE );
#endif
#if MB_TCP_ENABLED == 1
    eStatus = eMBTCPInit( MB_TCP_PORT_USE_DEFAULT );
#endif
    if (eStatus != MB_ENOERR )
      printf( "can't initialize modbus stack!\r\n" );

    /* Enable the Modbus Protocol Stack. */
    eStatus = eMBEnable(  );
    if (eStatus != MB_ENOERR )
      fprintf( stderr, "can't enable modbus stack!\r\n" );

  // Initialise some registers
  usRegInputBuf[1] = 0x1234;
  usRegInputBuf[2] = 0x5678;
  usRegInputBuf[3] = 0x9abc;        

  while(true)
  {
#if MB_TCP_ENABLED == 1
    Net::poll();
#endif

    if(tm.read()>.5)
    {
      led1=!led1; //Show that we are alive
      tm.start();
    }

    eStatus = eMBPoll(  );
            
    /* Here we simply count the number of poll cycles. */
    usRegInputBuf[0]++;
  }
  //return 0;
}

eMBErrorCode
eMBRegInputCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    int             iRegIndex;

    if( ( usAddress >= REG_INPUT_START )
        && ( usAddress + usNRegs <= REG_INPUT_START + REG_INPUT_NREGS ) )
    {
        iRegIndex = ( int )( usAddress - usRegInputStart );
        while( usNRegs > 0 )
        {
            *pucRegBuffer++ = ( unsigned char )( usRegInputBuf[iRegIndex] >> 8 );
            *pucRegBuffer++ = ( unsigned char )( usRegInputBuf[iRegIndex] & 0xFF );
            iRegIndex++;
            usNRegs--;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }
    return eStatus;
}

eMBErrorCode
eMBRegHoldingCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs, eMBRegisterMode eMode )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    int             iRegIndex;

    if( ( usAddress >= REG_HOLDING_START ) &&
        ( usAddress + usNRegs <= REG_HOLDING_START + REG_HOLDING_NREGS ) )
    {
        iRegIndex = ( int )( usAddress - usRegHoldingStart );
        switch ( eMode )
        {
            /* Pass current register values to the protocol stack. */
        case MB_REG_READ:
            while( usNRegs > 0 )
            {
                *pucRegBuffer++ = ( UCHAR ) ( usRegHoldingBuf[iRegIndex] >> 8 );
                *pucRegBuffer++ = ( UCHAR ) ( usRegHoldingBuf[iRegIndex] & 0xFF );
                iRegIndex++;
                usNRegs--;
            }
            break;

            /* Update current register values with new values from the
             * protocol stack. */
        case MB_REG_WRITE:
            while( usNRegs > 0 )
            {
                usRegHoldingBuf[iRegIndex] = *pucRegBuffer++ << 8;
                usRegHoldingBuf[iRegIndex] |= *pucRegBuffer++;
                iRegIndex++;
                usNRegs--;
            }
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }
    return eStatus;
}

/* 
 * Following implementation is not actually checked.
 */

eMBErrorCode
eMBRegCoilsCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNCoils, eMBRegisterMode eMode )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    int             iIntRegIndex;
    int                iIntBufNum;
    int                iIntBitNum;
    int                iExtRegIndex=0;
    int                iExtBufNum;
    int                iExtBitNum;
    UCHAR            ucTemp;
    if( ( usAddress >= REG_COIL_START )
        && ( usAddress + usNCoils <= REG_COIL_START + REG_COIL_NREGS ) )
    {
        iIntRegIndex = ( int )( usAddress - usRegCoilStart );

        while( usNCoils > 0 )
        {
            iIntBufNum=iIntRegIndex/8;
            iIntBitNum=iIntRegIndex%8;
            iExtBufNum=iExtRegIndex/8;
            iExtBitNum=iExtRegIndex%8;

            switch ( eMode )
            {
            case MB_REG_READ:
                // Read coils
                if(iExtBitNum==0){
                    pucRegBuffer[iExtBufNum]=0;
                }
                ucTemp=(usRegCoilBuf[iIntBufNum]>>iIntBitNum) & 1;
                pucRegBuffer[iExtBufNum]|=ucTemp<<iExtBitNum;
                break;

            case MB_REG_WRITE:
                // Write coils
				ucTemp=usRegCoilBuf[iIntBufNum]&(~(1<<iIntBitNum));
				ucTemp|=((pucRegBuffer[iExtBufNum]>>iExtBitNum) & 1)<<iIntBitNum;
				usRegCoilBuf[iIntBufNum]=ucTemp;
                break;
            }
            iIntRegIndex++;
            iExtRegIndex++;
            usNCoils--;

        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }

    return eStatus;
}

eMBErrorCode
eMBRegDiscreteCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNDiscrete )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    int             iIntRegIndex;
    int                iIntBufNum;
    int                iIntBitNum;
    int                iExtRegIndex=0;
    int                iExtBufNum;
    int                iExtBitNum;
    UCHAR            ucTemp;
    if( ( usAddress >= REG_DISC_START )
        && ( usAddress + usNDiscrete <= REG_DISC_START + REG_DISC_NREGS ) )
    {
        iIntRegIndex = ( int )( usAddress - usRegDiscStart );

        while( usNDiscrete > 0 )
        {
            iIntBufNum=iIntRegIndex/8;
            iIntBitNum=iIntRegIndex%8;
            iExtBufNum=iExtRegIndex/8;
            iExtBitNum=iExtRegIndex%8;

            // Read discrete inputs
            if(iExtBitNum==0){
                pucRegBuffer[iExtBufNum]=0;
            }
            ucTemp=(usRegDiscBuf[iIntBufNum]>>iIntBitNum) & 1;
            pucRegBuffer[iExtBufNum]|=ucTemp<<iExtBitNum;

            iIntRegIndex++;
            iExtRegIndex++;
            usNDiscrete--;

        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }

    return eStatus;
}
