/************************************************************************
* Copyright (C) 2001 Thomas Schulz					*
*									*
*	$Id: genproto.h,v 1.2 2001/01/21 23:38:37 tsch Rel $  		*
*									*
* header for general IC35 protocol support				*
*									*
************************************************************************/
#ifndef _GENPROTO_H
#define	_GENPROTO_H	1

#include "util.h"	/* ushort, ..	*/


/* communication error codes	*/
#define ERR_intr	-2	/* user interrupt with Ctl-C	*/
#define ERR_recv	-3	/* receive error		*/
#define ERR_chksum	-4	/* block checksum mismatch	*/
#define ERR_acknak	-5	/* ack/nak handshake failed	*/


void	putbyte(  uchar * pduptr, uchar  byte );
void	putword(  uchar * pduptr, ushort word );
void	putdword( uchar * pduptr, ulong  dword );
void	putbtxt(  uchar * pduptr, uchar * text, size_t tlen );
void	puttext(  uchar * pduptr, char *  text );
void	puttxt0(  uchar * pduptr, char *  text );

uchar	getbyte(  uchar * pduptr );
ushort	getword(  uchar * pduptr );
ulong	getdword( uchar * pduptr );
void	getbtxt(  uchar * pduptr, uchar * text, size_t tlen );
void	gettext(  uchar * pduptr, char *  text, size_t tlen );

ushort	chksum( uchar * data, size_t dlen );

int 	welcome( uchar cmd );

#endif /*_GENPROTO_H*/
