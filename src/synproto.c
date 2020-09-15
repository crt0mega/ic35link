/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*/
	static char rcsid[] =
	"$Id: synproto.c,v 1.11 2001/06/17 23:37:36 tsch Rel $";  	/*
*									*
* IC35 synchronize protocol						*
*									*
*************************************************************************
*									*
*	??? is "fixme" mark: sections of code needing fixes		*
*									*
************************************************************************/

#include <stdio.h>	/* sprintf(), ..	*/
#include <stdarg.h>	/* va_start(), ..	*/
#include <string.h>	/* memcpy(), strlen() ..*/
#include <time.h>	/* struct tm, time(),.. */
#include <sys/types.h>	/* size_t, ..		*/

#include "util.h"	/* ERR,OK, uchar, ..	*/
#include "comio.h"	/* com_send(), ..	*/
#include "genproto.h"	/* putxxx(),getxxx() .. */
#include "synproto.h"	/* Level-4 commands, ..	*/
NOTUSED(rcsid)


/* Level-1 PDU Ids	*/
#define L1INIT	0x01	/* initialize	  01 03 00		*/
#define L1DSEL	0x02	/* data select	  02 ll ll <data> cc cc	*/
#define L1DREQ	0x04	/* data request   04 03 00		*/
#define L1EXIT	0x05	/* exit		  05 03 00		*/
#define L1ACK0	0xF0	/* ack to initialize			*/
#define L1ACK1	0xF1	/* ack to datasel,exit			*/
#define L1DATA	0xF2	/* data response  F2 ll ll <data> cc cc	*/

/* Level-2 PDU Ids	*/
#define L2INIT	0x80	/* cmd,rsp: identification			*/
#define L2EXIT	0x81	/* cmd: disconnect				*/
#define	L2WMORE	0x02	/* cmd: write non-last of multiblk record	*/
#define	L2WLAST	0x82	/* cmd: write last of multi-block record	*/
#define L2READ	0x83	/* cmd: read more of multi-block record		*/
#define L2RMORE 0x20	/* rsp: non-last data of multi-block record	*/
#define L2RLAST 0xA0	/* rsp: last data of multi-block record		*/
#define L2RCONT 0x90	/* rsp: write more of multi-block record	*/

/* Level-3 PDU Ids	*/
#define L3MORE	0x48	/* non-last of multi-block			*/
#define L3LAST	0x49	/* last of multi-block or single		*/
#define L3IDENT	0x4A	/* identification				*/

/* Level-4 PDU definitions	*/

#define pdulenof(type,first,last)	\
			( offsetof(type,last) + sizeof(((type*)0)->last) \
			- offsetof(type,first) )

struct hdr {
    uchar	id;		/* id		*/
    uchar	len[2];		/* ll ll	*/
};
union recid {
    uchar	recid[4];	/* re_cd_id fi  IC35 record-ID	*/
    struct {
	uchar     rec[3];	/* re_cd_id  record-id part	*/
	uchar     file[1];	/* fi	     file-id part	*/
    }		id;
};

struct cmdident {
    struct hdr	l1head;		/* 02 ll ll		*/
    struct hdr	l2head;		/* 80 ll ll		*/
    uchar	magic[4];	/* 10 00 64 00				*/
    struct hdr	l3head;		/* 4A ll ll				*/
    char	text[28];	/* "INVENTEC CORPORATION PRODUCT"	*/
};
#define LCMDident	pdulenof(struct cmdident, magic, text)
struct rspident {
    struct hdr	l1head;		/* F2 ll ll		*/
    struct hdr	l2head;		/* A0 ll ll		*/
    uchar	magic[4];	/* 10 00 D0 07				*/
    struct hdr	l3head;		/* 4A ll ll				*/
    char	text[31];	/* "INVENTEC CORPORATION DCS15 1.28"	*/
};

struct cmdgetpower {
    uchar	cmd[2];		/* 03 01	*/
    char	text[5];	/* "Power"	*/
    uchar	zero[3];	/* 00 00 00	*/
};
#define LCMDgetpower	pdulenof(struct cmdgetpower, cmd, zero)
struct rspgetpower {
    uchar	rsp[2];		/* 03 01	*/
};
struct cmdpassword {
    uchar	cmd[2];		/* 03 00	*/
    char	text[8];	/* [password]	*/
};
#define LCMDpassword	pdulenof(struct cmdpassword, cmd, text)
struct rsppassword {
    uchar	rsp[2];		/* 01 01	*/
};
struct cmdcategory {
    uchar	cmd[2];		/* 03 02	*/
    char	name[8];	/* [category]	*/
};
#define LCMDcategory	pdulenof(struct cmdcategory, cmd, name)
struct rspcategory {
    uchar	rsp[2];		/* 00 01	*/
};
struct cmdgetdtime {
    uchar	cmd[2];		/* 02 00	*/
    uchar	zero[1];	/* 00		*/
};
#define LCMDgetdtime	pdulenof(struct cmdgetdtime, cmd, zero)
struct rspgetdtime {
    uchar	mdyhms[14];	/* [mmddyyyyhhmmss]	*/
    uchar	zero[2];	/* 00 00		*/
};
struct cmdsetdtime {
    uchar	cmd[2];		/* 02 01	*/
    uchar	zero1[1];	/* 00			*/
    char	mdyhms[14];	/* [mmddyyyyhhmmss]	*/
    uchar	zero2[2];	/* 00 00		*/
};
#define LCMDsetdtime	pdulenof(struct cmdsetdtime, cmd, zero2)
/*     rspsetdtime is done only */

struct cmdfopen {
    uchar	cmd[2];		/* 00 02	*/
    uchar	zero[7];	/* 00 00 00 00 00 00 00	*/
    uchar	lf2[4];		/* length+2 (dword)	*/
    uchar	lf[1];		/* length of filename	*/
    uchar	fname[10+1];	/* [filename] 02	*/
};
#define LCMDfopen	(pdulenof(struct cmdfopen, cmd, lf) + 1) /* dynamic */
struct rspfopen {
    uchar	fd[2];		/* fd __		*/
};
struct cmdfclose {
    uchar	cmd[2];		/* 00 03	*/
    uchar	fd[2];		/* fd __		*/
    uchar	zero[4];	/* 00 00 00 00		*/
};
#define LCMDfclose	pdulenof(struct cmdfclose, cmd, zero)
/*     rspfclose is done only	*/

struct cmdfgetlen {
    uchar	cmd[2];		/* 01 03  (or 01 04)	*/
    uchar	fd[2];		/* fd __		*/
    uchar	zero[4];	/* 00 00 00 00		*/
};
#define LCMDfgetlen	pdulenof(struct cmdfgetlen, cmd, zero)
struct rspfgetlen {
    uchar	n[2];		/* n_ __		*/
};
struct cmdfgetrec {
    uchar	cmd[2];		/* 01 06  (or 01 07)	*/
    uchar	fd[2];		/* fd __		*/
    uchar	index[2];	/* id x_  (not 01 07)	*/
    uchar	zero[2];	/* 00 00 00 00		*/
};
#define LCMDfgetrec	pdulenof(struct cmdfgetrec, cmd, zero)
struct cmdfgetirec {
    uchar	cmd[2];		/* 01 05			*/
    uchar	fd[2];		/* fd __			*/
    union recid uid;		/* re c- id fi  IC35 record-ID	*/
};
#define LCMDfgetirec	pdulenof(struct cmdfgetirec, cmd, uid)
struct rspfgetrec {
    union recid	uid;		/* re c. id fi  IC35 record-ID  */
    uchar	fx[1];		/* fx	  change-flag on IC35 ?	*/
    uchar	data[1];	/* flens,fdata.. (variable len)	*/
};
struct cmdfputrec {		/* putrec	or  updrec	*/
    uchar	cmd[2];		/* 01 08	    01 09	*/
    uchar	fd[2];		/* fd __		*/
    union recid uid;		/* 00 00 00 00  or  re c- id fi	*/
    uchar	magic[3];	/* m1 m2 m3		*/
    uchar	zero2[2];	/* 00 00		*/
    uchar	lr[4];		/* length of record (dword)	*/
    uchar	data[1];	/* flens,fdata.. (variable len)	*/
};
#define LCMDfputrec	pdulenof(struct cmdfputrec, cmd, lr)	/* dynamic */
struct rspfputrec {
    union recid uid;		/* re_cd_id fi  IC35 record-ID	*/
};
struct cmdfclrchg {		/* clrchg or delrec		*/
    uchar	cmd[2];		/* 01 08     01 02		*/
    uchar	fd[2];		/* fd __			*/
    union recid uid;		/* re c- id fi  IC35 record-ID	*/
};
#define LCMDfclrchg	pdulenof(struct cmdfclrchg, cmd, uid)
/*     rspfclrchg is done only	*/

struct cmdpdu {
    struct hdr	l1head;		/* 01,02,04,05		*/
    struct hdr	l2head;		/*    80,02,82,83,81	*/
    struct hdr	l3head;		/*    48,49		*/
    union {
	uchar		    cmd[2];
	struct cmdgetpower  getpower;
	struct cmdpassword  password;
	struct cmdcategory  category;
	struct cmdgetdtime  getdtime;
	struct cmdsetdtime  setdtime;
	struct cmdfopen     fopen;
	struct cmdfclose    fclose;
	struct cmdfgetlen   fgetlen;
	struct cmdfgetrec   fgetrec;
	struct cmdfgetirec  fgetirec;
	struct cmdfputrec   fputrec;
	struct cmdfclrchg   fclrchg;
    }		u;
};
struct rsppdu {
    struct hdr	l1head;		/* F0,F1,F2		*/
    struct hdr	l2head;		/*	 20,A0,90	*/
    struct hdr	l3head;		/*	 48,49		*/
    union {
	uchar		    rsp[2];
	struct rspgetpower  getpower;
	struct rsppassword  password;
	struct rspcategory  category;
	struct rspgetdtime  getdtime;
	/*     rspsetdtime  response done only	*/
	struct rspfopen     fopen;
	/*     rspfclose    response done only	*/
	struct rspfgetlen   fgetlen;
	struct rspfgetrec   fgetrec;
	struct rspfputrec   fputrec;
	/*     rspfclrchg   response done only	*/
    }		u;
};


/* ============================================ */
/*	utilities for PDU encode/decode		*/
/* ============================================ */

/* encode data items to PDU
*  ------------------------
*	putbyte() etc. imported from genproto.c
*/
static int
puthdr( uchar * pduptr, uchar id, int len )
{
    putbyte( pduptr+0, id );
    putword( pduptr+1, (ushort)(len+3) );
    return len+3;
}
static void
putcmd( uchar * pduptr, ushort cmd )
{
    putbyte( pduptr+0, (cmd & 0xFF00) >> 8 );	/* MSB first ..   */
    putbyte( pduptr+1,  cmd & 0x00FF );		/* LSB second (!) */
}

/* decode data items from PDU
*  --------------------------
*	getbyte() etc. imported from genproto.c
*/
static ushort
getrsp( uchar * pduptr )
{
    return (getbyte( pduptr+0 ) << 8) | getbyte( pduptr+1 );
}


/* ==================================== */
/*	Level-1 protocol		*/
/* ==================================== */
/*
*	the Level-1 protocol transports a command to IC35 (->)
*	and receives a response from IC35 (<-):
*	L2sendcmd:
*	use default timeout (0.5 sec)
*	 -> 01 03 00			init
*	 <- F0				ack0
*	 -> 02 ll ll <L2cmd> cc cc	command to IC35
*	set timeout 2.0 sec for ack1 response
*	 <- F1				ack1
*	restore previous timeout (0.5 sec)
*	L2recvrsp:
*	 -> 04 03 00			request response
*	 <- F2 ll ll <L2rsp> cc cc	response from IC35
*	 -> 05 03 00			exit
*	 <- F1				ack1
*	longer timeout 2.0 sec for ack1 response to command is needed
*	e.g. for get_modflen. experiments showed ack1 response times
*	of 0.7 .. 0.9 sec for ca. 860 address records.
*/

static uchar	xbuff[4096];		/* PDU transmit buffer	*/

/* send Level-1 PDU
*  ----------------
*/
static int
L1send( uchar id, uchar* pdu, size_t l2len )
{
    size_t	slen;
    uchar *	spdu;
    uchar	sbuff[3];

    if ( pdu != NULL ) {	/* long PDU with data and checksum	*/
	slen = puthdr( spdu = pdu, id, l2len+2 );
	putword( spdu+slen-2, chksum( spdu, slen-2 ) );
    } else {			/* short PDU without data,checksum	*/
	slen = puthdr( spdu = sbuff, id, 0 );
    }
    return com_send( spdu, slen );
}

/* receive Level-1 PDU
*  -------------------
*   returns:
*	<= 0	receive error: timeout, bad checksum of long PDU
*	== 0	short PDU received, *p_id = PDU-Id
*	> 0	long PDU received, *p_id = PDU-Id, *buff = PDU-data
*/
static int
L1recv( uchar* prid, uchar * pdu, size_t blen )
{
    int		rlen, pdulen;

    if ( prid == NULL )
	return ERR;				/* semantic error	*/
    *prid = '\0';
    rlen = com_recv( prid, sizeof(*prid) );
    if ( rlen < 1 )
	return rlen;				/* recv.ERR, timeout	*/
    if ( *prid != L1DATA )
	return OK;				/* received ack0,ack1	*/

    if ( pdu == NULL ) {
	/*??? flush receive data */
	return ERR;				/* semantic error	*/
    }
    pdu[0] = *prid;
    rlen = com_recv( pdu+1, 2 );
    if ( rlen < 2 )
	return ERR;				/* miss length field	*/
    pdulen = getword( pdu+1 );
    if ( pdulen <= 3 )
	return ERR;				/* too short data resp	*/
    rlen = com_recv( pdu+3, min(pdulen,blen)-3 );
    if ( rlen < pdulen - 3 )
	return ERR;				/* length mismatch	*/
    if ( chksum( pdu, pdulen-2 ) != getword( pdu+pdulen-2 ) )
	return ERR;				/* checksum mismatch	*/

    return pdulen - 3 - 2;			/* length of L2data	*/
}


/* send command with Level-1 protocol
*  ----------------------------------
*/
static int
L2sendcmd( uchar* pdu, size_t l2len )
{
    uchar	rid;
    int 	old_tmo;
    int 	rval;

    rval = OK;
    L1send( L1INIT, NULL, 0 );		/* -> 01 03 00			*/
    L1recv( &rid, NULL, 0 );		/* <- F0			*/
    if ( rid != L1ACK0 )		/*	missing ack0		*/
	return ERR;
    L1send( L1DSEL, pdu, l2len );	/* -> 02 ll ll <L2data> cc cc	*/
    old_tmo = com_settimeout( 2000 );	/* timeout 2.0 sec for ack1resp */
    L1recv( &rid, NULL, 0 );		/* <- F1			*/
    com_settimeout( old_tmo );		/* restore previous timeout	*/
    if ( rid != L1ACK1 )		/*	missing ack1		*/
	return ERR;
    return rval;
}

/* receive response with Level-1 protocol
*  --------------------------------------
*/
static int
L2recvrsp( uchar* rsp, size_t lrsp )
{
    int 	rlen, rval;
    uchar	rid;

    rval = ERR;
    L1send( L1DREQ, NULL, 0 );		/* -> 04 03 00			*/
    rlen = L1recv( &rid, rsp, lrsp );	/* <- F2 ll ll <L2data> cc cc	*/
    if ( rid == L1DATA )
	rval = rlen;			/* return length of L2data	*/
    L1send( L1EXIT, NULL, 0 );		/* -> 05 03 00			*/
    L1recv( &rid, NULL, 0 );		/* <- F1			*/
    /*??? ignore if missing ack1 to exit */
    return rval;
}


/* ==================================== */
/*	Level-4 commands,responses	*/
/* ==================================== */

/* encode command and send it
*  --------------------------
*	depending on the command-id 'cmd' zero, one or more parameters
*	are encoded.
*/
int
sendcmd( ushort cmd, ... )
{
    va_list		argp;
    struct cmdpdu *	pdu = (struct cmdpdu *)&xbuff[0];
    size_t		pdusize = sizeof(xbuff);
    size_t		pdulen;
    uchar		l2id, l3id;

    memset( pdu, 0, pdusize );
    putcmd( pdu->u.cmd, cmd );
    va_start( argp, cmd );
    switch ( cmd ) {
    default:
	return ERR;			/* invalid 'cmd'	*/
    case CMDident:
      { struct cmdident *idpdu = (struct cmdident *)pdu;
	char *		inventec = "INVENTEC CORPORATION PRODUCT";
	putbtxt( idpdu->magic, "\x10\x00\x64\x00", sizeof(idpdu->magic) );
	puttext( idpdu->text, inventec );
	puthdr( (uchar*)&idpdu->l3head, L3IDENT, strlen(inventec) );
	pdulen = LCMDident;
	l2id = L2INIT; l3id = 0;
      } break;
    case CMDdisconn:
	pdulen = 0;
	l2id = L2EXIT; l3id = 0;
	break;
    case CMDgetpower:
	puttext( pdu->u.getpower.text, "Power" );
	pdulen = LCMDgetpower;
	l2id = L2WLAST; l3id = L3LAST;
	break;
    case CMDpassword:
	puttext( pdu->u.password.text, va_arg( argp, char* ) );
	pdulen = LCMDpassword;
	l2id = L2WLAST; l3id = L3LAST;
	break;
    case CMDcategory:
	puttext( pdu->u.category.name, va_arg( argp, char* ) );
	pdulen = LCMDcategory;
	l2id = L2WLAST; l3id = L3LAST;
	break;
    case CMDgetdtime:
	pdulen = LCMDgetdtime;
	l2id = L2READ; l3id = L3LAST;
	break;
    case CMDsetdtime:
	puttext( pdu->u.setdtime.mdyhms, va_arg( argp, char* ) );
	pdulen = LCMDsetdtime;
	l2id = L2WLAST; l3id = L3LAST;
	break;
    case CMDfopen:
      { char *		strarg = va_arg( argp, char* );
	putdword( pdu->u.fopen.lf2, (ulong)(strlen(strarg) + 2) );
	putbyte( pdu->u.fopen.lf, (uchar)strlen(strarg) );
	puttext( pdu->u.fopen.fname, strarg );
	putbyte( pdu->u.fopen.fname+strlen(strarg), '\x02' );
	pdulen = LCMDfopen + strlen(strarg);
	l2id = L2WLAST; l3id = L3LAST;
      } break;
    case CMDfclose:
	putword( pdu->u.fclose.fd, (ushort)va_arg( argp, int ) );
	pdulen = LCMDfclose;
	l2id = L2WLAST; l3id = L3LAST;
	break;
    case CMDfgetlen:
    case CMDfgetmlen:
	putword( pdu->u.fgetlen.fd, (ushort)va_arg( argp, int ) );
	pdulen = LCMDfgetlen;
	l2id = L2READ; l3id = L3LAST;
	break;
    case CMDfgetirec:
	putword(  pdu->u.fgetirec.fd, (ushort)va_arg( argp, int ) );
	putdword( pdu->u.fgetirec.uid.recid, va_arg( argp, ulong ) );
	pdulen = LCMDfgetirec;
	l2id = L2READ; l3id = L3LAST;
	break;
    case CMDfgetrec:
	putword( pdu->u.fgetrec.fd, (ushort)va_arg( argp, int ) );
	putword( pdu->u.fgetrec.index, (ushort)va_arg( argp, int ) );
	pdulen = LCMDfgetrec;
	l2id = L2READ; l3id = L3LAST;
	break;
    case CMDfgetmrec:
	putword( pdu->u.fgetrec.fd, (ushort)va_arg( argp, int ) );
	pdulen = LCMDfgetrec;
	l2id = L2READ; l3id = L3LAST;
	break;
    case CMDfgetmore:
	pdulen = 0;
	l2id = L2READ; l3id = 0;
	break;
    case CMDfputrec:
    case CMDfupdrec:
      { bool		last	=	    va_arg( argp, bool );
	uchar * 	bstrarg;
	size_t		bstrlen;
	putword( pdu->u.fputrec.fd, (ushort)va_arg( argp, int ) );
	putdword( pdu->u.fputrec.uid.recid, va_arg( argp, ulong ) );
	putbtxt( pdu->u.fputrec.magic,	    va_arg( argp, uchar* ),
					    sizeof(pdu->u.fputrec.magic) );
	putdword( pdu->u.fputrec.lr,	    va_arg( argp, size_t ) );
	bstrarg =			    va_arg( argp, uchar* );
	bstrlen =			    va_arg( argp, size_t );
	putbtxt( pdu->u.fputrec.data, bstrarg, bstrlen );
	pdulen = LCMDfputrec + bstrlen;
	if ( ! last )
	    l2id = L2WMORE, l3id = L3MORE;
	else
	    l2id = L2WLAST, l3id = L3LAST;
      } break;
    case CMDfputmore:
      { bool		last	= va_arg( argp, bool );
	uchar * 	bstrarg = va_arg( argp, uchar* );
	size_t		bstrlen = va_arg( argp, size_t );
	putbtxt( pdu->u.cmd, bstrarg, bstrlen );
	pdulen = bstrlen;
	if ( ! last )
	    l2id = L2WMORE, l3id = L3MORE;
	else
	    l2id = L2WLAST, l3id = L3LAST;
      } break;
    case CMDfdelrec:
    case CMDfclrchg:
	putword( pdu->u.fclrchg.fd, (ushort)va_arg( argp, int ) );
	putdword( pdu->u.fclrchg.uid.recid, va_arg( argp, ulong ) );
	pdulen = LCMDfclrchg;
	l2id = L2WLAST; l3id = L3LAST;
	break;
    }
    va_end( argp );
    if ( l3id )
	pdulen = puthdr( (uchar*)&pdu->l3head, l3id, pdulen );
    pdulen = puthdr( (uchar*)&pdu->l2head, l2id, pdulen );
    return L2sendcmd( (uchar*)pdu, pdulen );
}

/* receive response and decode it
*  ------------------------------
* ???	check received l2id,l3id match with cmd acc.to protocol
*/
int
recvrsp( ushort cmd, ... )
{
    va_list		argp;
    uchar		l2id, l3id;
    ushort		rsp;
    int			pdulen;
    size_t		pdusize = sizeof(xbuff);
    struct rsppdu *	pdu = (struct rsppdu *)&xbuff[0];

    va_start( argp, cmd );
    memset( pdu, 0, pdusize );
    if ( (pdulen = L2recvrsp( (uchar*)pdu, pdusize )) < 0 )
	return pdulen;
    l2id = getbyte( (uchar*)&pdu->l2head );
    l3id = 0x00;
    if ( pdulen >= sizeof(pdu->l2head) ) {
	pdulen -= sizeof(pdu->l2head);
	l3id = getbyte( (uchar*)&pdu->l3head );
	if ( pdulen >= sizeof(pdu->l3head) )
	    pdulen -= sizeof(pdu->l3head);
    }
    rsp = getrsp( pdu->u.rsp );
    switch ( cmd ) {
    default:
	return ERR;			/* invalid 'cmd'	*/
    case CMDident:
      { struct rspident *idpdu = (struct rspident *)pdu;
	char *		ptext  = va_arg( argp, char* );
	size_t		lptext = va_arg( argp, size_t );
	pdulen -= sizeof(idpdu->magic);
	gettext( idpdu->text, ptext, min(pdulen, lptext) );
      } break;
    case CMDdisconn:
	break;
    case CMDgetpower:
      { ushort *	pstate = va_arg( argp, ushort* );
	*pstate = rsp;
      } break;
    case CMDpassword:
      { ushort *	pstate = va_arg( argp, ushort* );
	*pstate = rsp;
      } break;
    case CMDgetdtime:
      { char *		pdtime = va_arg( argp, char* );
	size_t		lpdtime = va_arg( argp, size_t );
	gettext( pdu->u.getdtime.mdyhms, pdtime,
		  min(sizeof(pdu->u.getdtime.mdyhms), lpdtime) );
      } break;
    case CMDsetdtime:
	break;
    case CMDcategory:
      { ushort *	pstate = va_arg( argp, ushort* );
	*pstate = rsp;
      } break;
    case CMDfopen:
      { ushort *	pfd = va_arg( argp, ushort* );
	*pfd = getword( pdu->u.fopen.fd );
      } break;
    case CMDfclose:
	break;
    case CMDfgetlen:
    case CMDfgetmlen:
      { ushort *	pflen = va_arg( argp, ushort* );
	*pflen = getword( pdu->u.fgetlen.n );
      } break;
    case CMDfgetrec:
      {	bool *		plast = va_arg( argp, bool* );
	ulong * 	prid  = va_arg( argp, ulong* );
	uchar * 	pchg  = va_arg( argp, uchar* );
	uchar * 	buff  = va_arg( argp, uchar* );
	size_t		blen  = va_arg( argp, size_t );
	*plast = (bool)( l2id == L2RLAST );
	*prid  = getdword( pdu->u.fgetrec.uid.recid );
	*pchg  = getbyte( pdu->u.fgetrec.fx );
	memcpy( buff, pdu->u.fgetrec.data, min(blen,pdulen) );
	pdulen -= offsetof(struct rspfgetrec, data); /* length of record data */
      } break;
    case CMDfgetmore:
      { bool *		plast = va_arg( argp, bool* );
	uchar *		buff  = va_arg( argp, uchar* );
	size_t		blen  = va_arg( argp, size_t );
	*plast = (bool)( l2id == L2RLAST );
	memcpy( buff, pdu->u.rsp, min(blen,pdulen) );
      } break;
    case CMDfputrec:
	if ( l2id == L2RLAST && l3id == L3LAST ) {
	    ulong *	prid = va_arg( argp, ulong* );
	    *prid = getdword( pdu->u.fputrec.uid.recid );
	}
	break;
    case CMDfdelrec:
    case CMDfclrchg:
	break;
    }
    return pdulen;
}
