/************************************************************************
* Copyright (C) 2001 Thomas Schulz					*
*									*/
	static char rcsid[] =
	"$Id: genproto.c,v 1.2 2001/01/21 19:54:52 tsch Rel $";  	/*
*									*
* general IC35 protocol support						*
*									*
*************************************************************************
*									*
*	??? is "fixme" mark: sections of code needing fixes		*
*									*
*		utilities for PDU encode/decode				*
*	putbyte 	encode one byte					*
*	putword 	encode 2byte-word				*
*	putdword	encode 4byte-doubleword				*
*	putbtxt 	encode binary text				*
*	puttext 	encode text string				*
*	puttxt0 	encode text string plus trailing NUL-byte	*
*	getbyte 	decode one byte					*
*	getword 	decode 2byte-word				*
*	getdword	decode 4byte-doubleword				*
*	getbtxt 	decode binary text				*
*	gettext 	decode text string				*
*	chksum		calculate arithmetic checksum			*
*		general communication					*
*	welcome 	initial welcome handshake			*
*									*
************************************************************************/

#include <string.h>	/* memcpy(), ..		*/
#include <unistd.h>	/* usleep()		*/
#include <signal.h>	/* SIG_INT, signal()	*/

#include "util.h"	/* ERR,OK, uchar, ..	*/
#include "comio.h"	/* com_send(), ..	*/
#include "genproto.h"
NOTUSED(rcsid)


/* ============================================ */
/*	utilities for PDU encode/decode		*/
/* ============================================ */

/* encode data items to PDU
*  ------------------------
*/
void
putbyte( uchar * pduptr, uchar byte )
{
    *pduptr = byte;
}
void
putword( uchar * pduptr, ushort word )
{
    putbyte( pduptr+0,  word & 0x00FF );	/* LSB first ..	*/
    putbyte( pduptr+1, (word & 0xFF00) >> 8 );	/* .. then MSB	*/
}
void
putdword( uchar * pduptr, ulong dword )
{
    putword( pduptr+0,  dword & 0x0000FFFF );
    putword( pduptr+2, (dword & 0xFFFF0000) >> 16 );
}
void
putbtxt( uchar * pduptr, uchar * text, size_t tlen )
{
    memcpy( pduptr, text, tlen );
}
void
puttext( uchar * pduptr, char * text )
{
    putbtxt( pduptr, text, strlen( text ) );
}
void
puttxt0( uchar * pduptr, char * text )
{
    size_t	len;

    putbtxt( pduptr, text, len = strlen( text ) );
    pduptr[len] = '\0';
}

/* decode data items from PDU
*  --------------------------
*/
uchar
getbyte( uchar * pduptr )
{
    return *pduptr;
}
ushort
getword( uchar * pduptr )
{
    return (getbyte( pduptr+1 ) << 8) | getbyte( pduptr+0 );
}
ulong
getdword( uchar * pduptr )
{
    return (getword( pduptr+2 ) << 16) | getword( pduptr+0 );
}
void
getbtxt( uchar * pduptr, uchar * text, size_t tlen )
{
    memcpy( text, pduptr, tlen );
}
void
gettext( uchar * pduptr, char * text, size_t tlen )
{
    getbtxt( pduptr, text, tlen );
    text[tlen] = '\0';
}

/* calculate arithmetic checksum
*  -----------------------------
*/
ushort
chksum( uchar * data, size_t dlen )
{
    ushort	checksum;
    size_t	i;

    for ( checksum = 0x0000, i = 0; i < dlen; ++i )
	checksum += data[i];
    return checksum;
}


/* ==================================== */
/*	general communication		*/
/* ==================================== */

/* welcome handshake
*  -----------------
*	send 'cmd' every 1.15 sec until "WELCOME" received
*	send 'cmd', wait for '\x80'
*	command byte 'cmd' is for:
*	- synchronize protocol '\x41'='A'
*	- manager protocol '\x40'='@'
*/
#define WELCOME_TIMEOUT 1150
#define WELCOME_RETRY	20

#define RSP_READY	(uchar)0x80
#define GOT_NONE	0
#define GOT_WELCOME	1
#define GOT_READY 	2

static int		hadsignal;

static void
_sig_hdlr( int signum )
{
    hadsignal = signum;
}

int
welcome( uchar cmd )
{
    char *	welcome = "WELCOME";
    int 	old_tmo;
    int 	retry, rlen;
    int		state;
    uchar	rbuff[7];

    hadsignal = 0;
    signal( SIGINT, _sig_hdlr );
    old_tmo = com_settimeout( WELCOME_TIMEOUT );
    for ( retry = 0, state = GOT_NONE;
	  retry < WELCOME_RETRY && state != GOT_READY && hadsignal == 0;
	  ++retry ) {
	com_send( &cmd, 1 );
	switch ( state ) {
	case GOT_NONE:
	    rlen = com_recv( rbuff, sizeof(rbuff) );
	    if ( rlen < strlen(welcome)	/* receive error / timeout	*/
	      || memcmp( rbuff, welcome, strlen(welcome) ) != 0 )
		continue;		/* not enough or miss "WELCOME" */
	    state = GOT_WELCOME;
	    continue;
	case GOT_WELCOME:
	    rlen = com_recv( rbuff, 1 );
	    if ( rlen < 1		/* receive error / timeout	*/
	      || rbuff[0] != RSP_READY )
		continue;		/* missing RSP_READY		*/
	    state = GOT_READY;
	    break;
	}
	break;
    }
    signal( SIGINT, SIG_DFL );
    com_settimeout( old_tmo );
    if ( hadsignal )
	return ERR_intr;
    if ( state != GOT_READY )
	return ERR;
    usleep( 50000 );			/* give IC35 50ms to get ready	*/
    return OK;
}
