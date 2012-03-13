/*
 * FreeModbus Libary: mbed Port
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id: porttcp.c,v 1.1 2006/08/30 23:18:07 wolti Exp $
 */
/* ----------------------- System includes ----------------------------------*/
#include <stdio.h>
#include <string.h>

#include "port.h"

/* ----------------------- mbed includes ------------------------------------*/
#include "mbed.h"
#include "EthernetNetIf.h"
#include "TCPSocket.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"

/* ----------------------- MBAP Header --------------------------------------*/
#define MB_TCP_UID          6
#define MB_TCP_LEN          4
#define MB_TCP_FUNC         7

/* ----------------------- Defines  -----------------------------------------*/
#define MB_TCP_DEFAULT_PORT 502 /* TCP listening port. */
#define MB_TCP_BUF_SIZE     ( 256 + 7 ) /* Must hold a complete Modbus TCP frame. */

/* ----------------------- Prototypes ---------------------------------------*/
void prvvMBPortReleaseClient(  );
BOOL prvbMBPortAcceptClient(  );
BOOL prvMBTCPGetFrame(  );
void onListeningTCPSocketEvent(TCPSocketEvent e);
void onConnectedTCPSocketEvent(TCPSocketEvent e);

/* ----------------------- Static variables ---------------------------------*/
static UCHAR    aucTCPBuf[MB_TCP_BUF_SIZE];
static USHORT   usTCPBufPos;
static USHORT   usTCPFrameBytesLeft;

static TCPSocket ListeningSock;
static TCPSocket* pConnectedSock; // for ConnectedSock
static Host client;

/* ----------------------- Begin implementation -----------------------------*/
BOOL
xMBTCPPortInit( USHORT usTCPPort )
{
    BOOL            bOkay = FALSE;
    USHORT          usPort;

    if( usTCPPort == 0 )
    {
        usPort = MB_TCP_DEFAULT_PORT;
    }
    else
    {
        usPort = ( USHORT ) usTCPPort;
    }
    pConnectedSock=NULL;

    // Set the callbacks for Listening
    ListeningSock.setOnEvent(&onListeningTCPSocketEvent); 

    // bind and listen on TCP
    if( ListeningSock.bind(Host(IpAddr(), usPort)) )
    {
        // Failed to bind
        bOkay = FALSE;
    }
    else if( ListeningSock.listen() )
    {
        // Failed to listen
        bOkay = FALSE;
    }
    else
    {
#ifdef MB_TCP_DEBUG
        vMBPortLog( MB_LOG_DEBUG, "MBTCP-ACCEPT", "Protocol stack ready.\r\n" );
#endif
        bOkay = TRUE;
    }    

    return bOkay;
}

void
vMBTCPPortClose(  )
{
    /* Shutdown any open client sockets. */
    prvvMBPortReleaseClient();

    /* Shutdown or listening socket. */
    ListeningSock.close();

    /* Release resources for the event queue. */
//    vMBPortEventClose(  );
}

void
vMBTCPPortDisable( void )
{
    prvvMBPortReleaseClient( );
}

BOOL
xMBTCPPortGetRequest( UCHAR ** ppucMBTCPFrame, USHORT * usTCPLength )
{
    *ppucMBTCPFrame = &aucTCPBuf[0];
    *usTCPLength = usTCPBufPos;

    /* Reset the buffer. */
    usTCPBufPos = 0;
    usTCPFrameBytesLeft = MB_TCP_FUNC;
    return TRUE;
}

BOOL
xMBTCPPortSendResponse( const UCHAR * pucMBTCPFrame, USHORT usTCPLength )
{
    BOOL            bFrameSent = FALSE;

    if( pConnectedSock )
    {
        if(pConnectedSock->send((char *)pucMBTCPFrame, usTCPLength)>=0)
        {
            bFrameSent = TRUE;
            printf("sent %d bytes\n",usTCPLength);
        }
        else
        {
            /* Drop the connection in case of an write error. */
            printf("sent error!\n");
            prvvMBPortReleaseClient( );
        }
    }
    return bFrameSent;
}

void
prvvMBPortReleaseClient(  )
{
    if(pConnectedSock){
        IpAddr clientIp = client.getIp();
#ifdef MB_TCP_DEBUG
        vMBPortLog( MB_LOG_DEBUG, "MBTCP-CLOSE", "Closed connection to %d.%d.%d.%d.\r\n",
             clientIp[0], clientIp[1], clientIp[2], clientIp[3]);
#endif
        pConnectedSock->close();
        pConnectedSock=NULL;
    }
}


BOOL prvbMBPortAcceptClient(  )
{
    // Accepts connection from client and gets connected socket.   

    if (ListeningSock.accept(&client, &pConnectedSock)) {
        return FALSE; //Error in accept, discard connection
    }
    // Setup the new socket events
    pConnectedSock->setOnEvent(&onConnectedTCPSocketEvent);
    // We can find out from where the connection is coming by looking at the
    // Host parameter of the accept() method
    IpAddr clientIp = client.getIp();

#ifdef MB_TCP_DEBUG
    vMBPortLog( MB_LOG_DEBUG, "MBTCP-ACCEPT", "Accepted new client %d.%d.%d.%d\r\n",
        clientIp[0], clientIp[1], clientIp[2], clientIp[3]);
#endif

    usTCPBufPos = 0;
    usTCPFrameBytesLeft = MB_TCP_FUNC;

    return TRUE;
}


BOOL
prvMBTCPGetFrame(  )
{
    BOOL            bOkay = TRUE;
    USHORT          usLength;
    int             iRes;
    int i;
    int total=0;
    int len;
    char buf[MB_TCP_BUF_SIZE];
    /* Make sure that we can safely process the next read request. If there
     * is an overflow drop the client.
     */
    
    if( ( usTCPBufPos + usTCPFrameBytesLeft ) >= MB_TCP_BUF_SIZE )
    {
        // buffer overrun
        return FALSE;
    }

    while (len = pConnectedSock->recv((char *)&aucTCPBuf[usTCPBufPos], MB_TCP_BUF_SIZE-usTCPBufPos) ) {
        usTCPBufPos+=len;
        usTCPFrameBytesLeft-=len;
    }

    /* If we have received the MBAP header we can analyze it and calculate
     * the number of bytes left to complete the current request. If complete
     * notify the protocol stack.
     */
    if( usTCPBufPos >= MB_TCP_FUNC )
    {
        /* Length is a byte count of Modbus PDU (function code + data) and the
         * unit identifier. */
        usLength = aucTCPBuf[MB_TCP_LEN] << 8U;
        usLength |= aucTCPBuf[MB_TCP_LEN + 1];

        /* Is the frame already complete. */
        if( usTCPBufPos < ( MB_TCP_UID + usLength ) )
        {
            usTCPFrameBytesLeft = usLength + MB_TCP_UID - usTCPBufPos;
        }
        /* The frame is complete. */
        else if( usTCPBufPos == ( MB_TCP_UID + usLength ) )
        {
#ifdef MB_TCP_DEBUG
            prvvMBTCPLogFrame( "MBTCP-RECV", &aucTCPBuf[0], usTCPBufPos );
#endif
            ( void )xMBPortEventPost( EV_FRAME_RECEIVED );
        }
        else
        {
#ifdef MB_TCP_DEBUG
            vMBPortLog( MB_LOG_DEBUG, "MBTCP-ERROR",
                            "Received too many bytes! Droping client.[%d>%d]\r\n" ,usTCPBufPos, MB_TCP_UID + usLength);
#endif
            /* This should not happen. We can't deal with such a client and
             * drop the connection for security reasons.
             */
            prvvMBPortReleaseClient(  );
        }
    }
    return bOkay;
}


void onListeningTCPSocketEvent(TCPSocketEvent e)
{
    switch(e)
    {
    case TCPSOCKET_ACCEPT:
        if(!prvbMBPortAcceptClient())
        {
#ifdef MB_TCP_DEBUG
            vMBPortLog( MB_LOG_DEBUG, "MBTCP-ERROR", "Error with client connection! Droping it.\r\n" );
#endif
            ListeningSock.close();
        }
        break;

    default:
#ifdef MB_TCP_DEBUG
        vMBPortLog( MB_LOG_DEBUG, "MBTCP-ERROR", "Unexpeted condition!.\r\n" );
#endif
        ListeningSock.close();
        break;
     };
}

void onConnectedTCPSocketEvent(TCPSocketEvent e)
{
    switch(e)
    {
    case TCPSOCKET_READABLE:
        if(!prvMBTCPGetFrame())prvvMBPortReleaseClient();
        break;
    case TCPSOCKET_CONNECTED:
    case TCPSOCKET_WRITEABLE:
        break;

    case TCPSOCKET_CONTIMEOUT:
    case TCPSOCKET_CONRST:
    case TCPSOCKET_CONABRT:
    case TCPSOCKET_ERROR:
    case TCPSOCKET_DISCONNECTED:
        prvvMBPortReleaseClient();
        break;
    default:
        break;
    }
}
