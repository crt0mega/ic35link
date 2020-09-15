/************************************************************************
* Copyright (C) 2001 Thomas Schulz					*
*									*/
	static char rcsid[] =
	"$Id: mgrproto.c,v 1.17 2001/11/20 23:08:35 thosch Exp $";  	/*
*									*
* IC35 manager protocol							*
*									*
*************************************************************************
*									*
*	??? is "fixme" mark: sections of code needing fixes		*
*									*
*		IC35 manager protocol					*
*	Mcmdrsp		send command-byte, get+check response-byte	*
*	Msendblk	send datablock, test checksum, ack/retry	*
*	Mrecvblk	receive datablock, test checksum, ack/retry	*
*		MMCard access protocol					*
*	MMCsend 	send MMCard command using Msendblk		*
*	MMCrecv 	receive MMCard response	using Mrecvblk		*
*		MMCard commands						*
*	MMCgetstatus	check if MMCard present / absent		*
*	MMCgetlabel	get MMCard label				*
*	MMCdiropen	open MMCard directory				*
*	MMCdirgetlen	get number of entries in MMCard directory	*
*	MMCdirread	read MMCard directory entry			*
*	MMCdirclose	close MMCard directory				*
*	MMCfiledel	delete MMCard file				*
*	MMCfileopen	open MMCard file				*
*	MMCfilestat	get MMCard file attributes			*
*	MMCfileread	read block from MMCard file			*
*	MMCfilewrite	write block to MMCard file			*
*	MMCfileclose	close MMCard file				*
*									*
************************************************************************/

#include <stdlib.h>	/* malloc(), ..		*/
#include <string.h>	/* memcpy(), ..		*/

#include "util.h"	/* ERR,OK, uchar, ..	*/
#include "comio.h"	/* com_send(), ..	*/
#include "genproto.h"	/* putxxx(),getxxx() .. */
#include "mgrproto.h"
NOTUSED(rcsid)

#define MAXRETRY	 5	/* max.retries for manager protocol	*/


/* ==================================== */
/*	IC35 manager protocol		*/
/* ==================================== */

/* send command-byte, get+check response-byte
*  ------------------------------------------
*/
int
Mcmdrsp( uchar cmd, uchar rsp )
{
    uchar	rbyte;
    int 	retry;

    for ( retry = 0; retry < MAXRETRY; ++retry ) {
	com_send( &cmd, sizeof(cmd) );
	if ( com_recv( &rbyte, sizeof(rbyte) ) == sizeof(rbyte)
	  && rbyte == rsp )
	    return OK;
    }
    return ERR;
}

/* local: send/receive block and ack or nak,retry
*  ----------------------------------------------
*	used by Msendblk(), Mrecvblk() for ack- or nak-handshake and retry,
*	the trasnsmit function _sendblk(), _recvblk() is passed as function
*	pointer 'pfxmit' and will set the appropriate timeout(s).
*/
static int
_xmitblk( uchar * data, size_t dlen, int pfxmit(uchar* data, size_t dlen) )
{
    int 	old_tmo;
    int 	retry;
    int 	rval = ERR;

    com_settimeout( old_tmo = com_settimeout( 0 ) );
    for ( retry = 0; retry < MAXRETRY+1; ++retry ) {
	rval = (*pfxmit)( data, dlen );		/* xmit data, recv csum */
	if ( rval == dlen ) {			/* datablock xmit OK	*/
	    if ( Mcmdrsp( MCMDposack, MRSPgotack ) != OK )
		rval = ERR_acknak;
	    break;
	} else if ( rval != ERR_recv		/* bad checksum/length	*/
		 && retry < MAXRETRY ) {	/*  and retries left	*/
	    if ( Mcmdrsp( MCMDnegack, MRSPgotack ) != OK ) {
		rval = ERR_acknak;
		break;
	    }
	    continue;
	} else					/* recv.err / timeout	*/
	    break;				/*  or out of retries	*/
    }
    com_settimeout( old_tmo );
    return rval;
}

/* send block and ack/retry
*  ------------------------
*	send database block, receive checksum and test it, on success send
*	acknowledge, receive ready response and return OK.
*	on failure send nak and retry, after max.retries return ERR.
*	 -> (block of 'dlen' bytes)
*	set timeout 10.0 sec
*	 <- cc_cc	got them, checksum is cc_cc (arithmetic, LSB first)
*	set timeout 3.0 sec
*	 -> 60		positive acknowledge
*	 <- A0		got ack
*	on checksum mismatch send negative acknowledge:
*	 -> 62		negative acknowledge
*	 <- A0		got nak
*	and restart send block.
*	the long "(block of 'dlen' bytes)" must be sent with waiting!
*/
static int
_sendblk( uchar * data, size_t dlen )
{
    uchar	rcsum[2];
    int 	lcsum;
    ushort	cksum;
    int 	rval;

    com_sendw( data, dlen );			    /* -> data (with wait) */
    com_settimeout( 10000 );		/* timeout 10.0 sec for chksum	*/
    if ( (lcsum = com_recv( rcsum, sizeof(rcsum) )) /* <- checksum	*/
	  == sizeof(rcsum)
      && (cksum = chksum( data, dlen ))
	  == getword( rcsum ) ) {
	rval = dlen;
    } else {
	if ( lcsum == sizeof(rcsum) ) { 	/* checksum mismatch	*/
	    LPRINTF(( L_NOISE, "_senddblk: chksum=%04X mismatch", cksum ));
	    rval = ERR_chksum;
	} else					/* recv.error / timeout */
	    rval = ERR_recv;
    }
    LPRINTF(( L_NOISE, "_sendblk(data,%d) = %d", dlen, rval ));
    com_settimeout( 3000 );		/* timeout 3.0 sec ack/nak resp	*/
    return rval;
}
int
Msendblk( uchar * data, size_t dlen )
{
    return _xmitblk( data, dlen, _sendblk );
}

/* receive block and ack/retry
*  ---------------------------
*	receive data block, on success acknowledge and return OK.
*	on failure send nak and retry, after max.retries return ERR.
*	set timeout 2.0 sec
*	 <- (block of 'blen' bytes) cc_cc	(arithmetic sum, LSB first)
*	 -> 60		positive acknowledge
*	 <- A0		got ack
*	on mismatch of checksum or length, send negative acknowledge:
*	 -> 62		negative acknowledge
*	 <- A0		got nak
*	and restart receive block.
*/
static int
_recvblk( uchar * buff, size_t blen )
{
    uchar	rcsum[2];
    ushort	cksum;
    int 	lcsum;
    int 	rval;

    rcsum[0] = rcsum[1] = (uchar)0x00;
    com_settimeout( 2000 );		/* timeout 2.0 sec for data,ack */
    if ( (rval  = com_recv( buff, blen )) >= 2		     /* <- data */
      && (lcsum = com_recv( rcsum, sizeof(rcsum)) ) >= 0 ) { /* <- csum */
	if (      lcsum == 1 ) {
	    rcsum[1] = buff[--rval];		/* take ..		*/
	    rcsum[0] = buff[--rval];		/* .. checksum bytes .. */
	} else if ( lcsum == 0 ) {
	    rcsum[1] = rcsum[0];		/* .. from ..		*/
	    rcsum[0] = buff[--rval];		/* .. end of datablock	*/
	}
	if ( (cksum = chksum( buff, rval )) != getword( rcsum ) ) {
	    LPRINTF(( L_NOISE, "_recvblk: chksum=%04X mismatch", cksum ));
	    rval = ERR_chksum;			/* checksum mismatch	*/
	}
    } else
	rval = ERR_recv;			/* too short for chksum */
    LPRINTF(( L_NOISE, "_recvblk(buff,%d) = %d", blen, rval ));
    return rval;
}
int
Mrecvblk( uchar * buff, size_t blen )
{
    return _xmitblk( buff, blen, _recvblk );
}


/* ==================================== */
/*	MMCard access protocol		*/
/* ==================================== */
/*
* protocol of MMCard operations
*   MMCsend
*	use timeout 0.5 sec
*	 -> 15		are you there ?
*	 <- 90		yes i am here !
*	 -> nn_nn	expect nn_nn bytes from me
*	 <- E0		ok, send them
*     sendretry:
*	 -> (block of nn_nn bytes)
*	set timeout 10.0 sec
*	 <- cc_cc	got them, checksum is cc_cc (arithmetic, LSB first)
*	set timeout 3.0 sec
*	 -> 60		ack
*	 <- A0		got ack
*	on checksum mismatch send negative acknowledge:
*	 -> 62		nak
*	 <- A0		got nak
*	and restart at sendretry.
*	the long "(block of nn_nn bytes)" must be sent with waiting!
*	the blocklength nn_nn must be sent with wait before 1st byte!
*   MMCrecv
*	set timeout 5.0 sec
*	 <- nn_nn	expect nn_nn bytes plus trailing checksum
*	 -> E0		ok, send them
*     recvretry:
*	 <- (block of nn_nn bytes) cc_cc	(arithmetic sum, LSB first)
*	 -> 60		ack
*	 <- A0		got ack
*	on mismatch of checksum or length, send negative acknowledge:
*	 -> 62		nak
*	 <- A0		got nak
*	and restart at recvretry.
*/

/* send MMCard command
*  -------------------
*/
static int
MMCsend( uchar * sbuff, size_t slen )
{
    uchar	rchar;
    uchar	slbuff[2];
    int 	rval;

    if ( Mcmdrsp( MCMDmmcard, MRSPgotcmd ) == OK ) {  /* -> 15		*/
	putword( slbuff, slen );		      /* <- 90		*/
	com_sendw( slbuff, sizeof(slbuff) ); 	      /* -> nn_nn (with wait) */
	if ( com_recv( &rchar, 1 ) == 1 	      /* <- E0		*/
	  && rchar == MRSPgotlen ) {
	    rval = Msendblk( sbuff, slen );	/* send cmd, ack/retry	*/
	    if ( rval != slen ) 	/* Msendblk() bad length	*/
		rval = ERR;
	} else				/* com_recv() MRSPgotlen failed */
	    rval = ERR;
    } else				/* Mcmdrsp() failed		*/
	rval = ERR;
    return rval;
}

/* receive MMCard response
*  -----------------------
*/
static int
MMCrecv( uchar ** prbuff )
{
    int 	old_tmo;
    uchar	rlbuff[2];
    size_t	rblen;
    uchar	rspgotlen = MRSPgotlen;
    uchar *	rbuff;
    int 	rval;

    old_tmo = com_settimeout( 5000 );
    if ( (rval = com_recv( rlbuff, sizeof(rlbuff) )) == sizeof(rlbuff) ) {
	rblen = getword( rlbuff );		/* <- nn nn	*/
	if ( (rbuff = calloc( rblen, 1 )) != NULL ) {
	    com_send( &rspgotlen, 1 );		/* -> E0	*/
	    rval = Mrecvblk( rbuff, rblen );	/* recv resp, ack/retry */
	    if ( rval == rblen ) {
		*prbuff = rbuff;		/* OK, buffer to caller */
	    } else {
		free( rbuff );		/* Mrecvblk() bad length	*/
		rval = ERR;
	    }
	} else {			/* calloc(rblen,1) failed	*/
	    LPRINTF(( L_ERROR, "MMCrecv: calloc(%d,1) failed", rblen ));
	    rval = ERR;
	}
    } else				/* com_recv(rlbuff,) failed	*/
	rval = ERR;
    com_settimeout( old_tmo );
    return rval;
}


/* ============================ */
/*	MMCard commands 	*/
/* ============================ */

/*
* MMCard command implementation
*	the MMCard commands correspond to IC35 SDK API functions and are
*	implemented as remote procedure calls: encode arguments to command
*	PDU and send it to IC35, receive response PDU from IC35 and decode
*	results from PDU.
*	struct mcmdxxxx/mrspxxxx define the command/response PDU encoding.
*	all MMCxxxx() functions work like:
*	- _init*pdu() allocates the command PDU buffer and encodes the
*	  command code into it
*	- encode MMCxxxx() function argument(s) into command PDU buffer
*	  (except fdstat argument)
*	- _sendrecv() optionally encodes the MMCxxxx() fdstat argument
*	  into the command PDU for file/directory commands,
*	  sends the command PDU to IC35 with MMCsend() and releases it,
*	  receives the response PDU from IC35 with MMCrecv(),
*	  optionally decodes FILE_IDEN from response PDU back into the
*	  fdstat argument,
*	  and returns the status decoded from the response PDU.
*	- decode MMCxxxx() result arguments
*	- decode from the response PDU into MMCxxxx() result arguments,
*	  release response PDU buffer and return status from _sendrecv().
*/

/* MMCard command codes */	/* IC35 SDK API function:		*/
#define MMCMDgetstatus	0x20	/* 13.1  mInitialCard() 		*/
#define MMCMDgetlabel	0x34	/* 13.4  mGetCardLabel()		*/
#define MMCMDdiropen	0x2A	/* 13.14 mOpenDirectory()		*/
#define MMCMDdirgetlen	0x2B	/* 13.15 mGetDirectorySubItemNum()	*/
#define MMCMDdirread	0x2C	/* 13.16 mGetDirectorySubItem() 	*/
#define MMCMDdirclose	0x2E	/* 13.18 mCloseDirectory()		*/
#define MMCMDfileopen	0x22	/* 13.6  mOpenFile()			*/
#define MMCMDfilestat	0x26	/* 13.10 mGetFileInfo() 		*/
#define MMCMDfilewrite	0x23	/* 13.8  mWriteToFile() 		*/
#define MMCMDfileread	0x24	/* 13.9  mReadFromFile()		*/
#define MMCMDfileclose	0x27	/* 13.11 mCloseFile()			*/
#define MMCMDfiledel	0x28	/* 13.13 mDeleteFile()			*/

/* MMCard PDU definitions
*  ----------------------
*/
/*	getstatus, getlabel */
struct mcmdstatlbl {		/* stat   label 	*/
    char	cmd[1]; 	/* 20  or  34		*/
    char	mmcard[6];	/* "MMCard"		*/
    char	mmcnum[1];	/* "1"  or  "2"		*/
    uchar	zero[1];	/* 00			*/
};
struct mrspstat {
    uchar	stat[2];	/* 01 00  or  FF FF	*/
};
struct mrsplbl {
    uchar	stat[2];	/* 01 00		*/
    uchar	zero[1];	/* 00			*/
    char	label[8+1];	/* [MMClabel] 00	*/
    char	reserved[11];	/* 20 20 00 00 00 00 00 00 00 00 48 */
};
/*	diropen, fileopen */
struct mcmdfdopen {		/* dopen   fopen	*/
    char	cmd[1]; 	/* 2A  or  22		*/
    uchar	mode[2];	/* 01 00		*/
    uchar	path[1];	/* [MMCardx\dir] 00	*/
};
struct mrspfdopen {
    uchar	stat[2];	/* 01 00		*/
    uchar	iden[FIDENSZ];	/* (27 bytes FILE_IDEN) */
};
/*	dirglen, dirclose, filestat, fileclose */
struct mcmdfdstatcl {		/* dglen dcls fstat fcls*/
    char	cmd[1]; 	/* 2B    2E   26    27	*/
    uchar	iden[FIDENSZ];	/* (27 bytes FILE_IDEN) */
    uchar	zero[1];	/* 00			*/
};
struct mrspdirglen {
    uchar	iden[FIDENSZ];	/* (27 bytes FILE_IDEN) */
    uchar	stat[2];	/* 01 00		*/
    uchar	ndent[2];	/* nn_nn		*/
};
struct mrspfdclose {
    uchar	iden[FIDENSZ];	/* (27 bytes FILE_IDEN) */
};
/*	dirread, fileread */
struct mcmdfdread {		/* dread   fread	*/
    uchar	cmd[1];		/* 2C  or  24		*/
    uchar	iden[FIDENSZ];	/* (27 bytes FILE_IDEN) */
    union {
      uchar	  index[2];	/* in_dx   dir-index	*/
      uchar	  blen[2];	/* nn_nn   file-bufflen */
    }		u;
    uchar	zero[1];	/* 00			*/
};
struct mrspfdstat {
    uchar	iden[FIDENSZ];	/* (27 bytes FILE_IDEN) */
    uchar	stat[2];	/* 01 00		*/
    FILE_INFO	dirent;		/* (24 bytes FILE_INFO) */
};
struct mrspfileread {
    uchar	iden[FIDENSZ];	/* (27 bytes FILE_IDEN) */
    uchar	stat[2];	/* 01 00		*/
    uchar	rlen[2];	/* nn_nn		*/
    uchar	data[1];	/* filedata[nn_nn]	*/
};
/*	filedel */
struct mcmdfiledel {
    char	cmd[1]; 	/* 28			*/
    uchar	path[1];	/* [MMCardx\file] 00	*/
};
/*resp mrspstat
*/
/*	filewrite */
struct mcmdfilewrite {
    uchar	cmd[1];		/* 23			*/
    uchar	iden[FIDENSZ];	/* (27 bytes FILE_IDEN) */
    uchar	wlen[2];	/* nn_nn		*/
    uchar	data[1];	/* filedata[nn_nn] 00	*/
};
struct mrspfilewrite {
    uchar	iden[FIDENSZ];	/* (27 bytes FILE_IDEN) */
    uchar	stat[2];	/* 01 00		*/
};

union mcmdpdu {			/* MMCard command PDU	*/
    uchar		cmd;
    struct mcmdstatlbl  status;
    struct mcmdstatlbl  label;
    struct mcmdfdopen   diropen;
    struct mcmdfdstatcl dirglen;
    struct mcmdfdread   dirread;
    struct mcmdfdstatcl dirclose;
    struct mcmdfiledel   filedel;
    struct mcmdfdopen    fileopen;
    struct mcmdfdstatcl  filestat;
    struct mcmdfdread    fileread;
    struct mcmdfilewrite filewrite;
    struct mcmdfdstatcl  fileclose;
    uchar		data[1];
};
union mrsppdu {			/* MMCard response PDU	*/
    uchar		stat[2];
    struct mrspstat     status;
    struct mrsplbl      label;
    struct mrspfdopen   diropen;
    struct mrspdirglen  dirglen;
    struct mrspfdstat   dirread;
    struct mrspfdclose  dirclose;
    struct mrspstat      filedel;
    struct mrspfdopen    fileopen;
    struct mrspfdstat    filestat;
    struct mrspfileread  fileread;
    struct mrspfilewrite filewrite;
    struct mrspfdclose   fileclose;
    uchar		data[1];
};
union mmcpdu {			/* generic MMCard PDU	*/
    union mcmdpdu	cmd;
    union mrsppdu	rsp;
};


/* MMCard PDU table
*  ----------------
*/
#define SPDULENOF(type) 	sizeof( struct type )
#define FIDENOFFS(type) 	offsetof( struct type, iden )
#define RSTATOFFS(type) 	offsetof( struct type, stat )
#define NOFFS			0xFFFF

struct mpdudesc {		/* MMCard PDU description	*/
    uchar	cmd;		/*  command code		*/
    size_t	slen;		/*  fixed length of PDU 	*/
    size_t	sidenoffs;	/*  offset FILE_IDEN in cmd PDU	*/
    size_t	rstatoffs;	/*  offset status in resp.PDU	*/
    size_t	ridenoffs;	/*  offset FILE_IDEN in rsp PDU */
};
struct mpdudesc	mmcpdutab[] = {
    { MMCMDgetstatus, SPDULENOF(mcmdstatlbl),	NOFFS,
		      RSTATOFFS(mrspstat),	NOFFS,			 },
    { MMCMDgetlabel,  SPDULENOF(mcmdstatlbl),	NOFFS,
		      RSTATOFFS(mrsplbl),	NOFFS,			 },
    { MMCMDdiropen,   SPDULENOF(mcmdfdopen),	NOFFS,
		      RSTATOFFS(mrspfdopen),	FIDENOFFS(mrspfdopen)    },
    { MMCMDdirgetlen, SPDULENOF(mcmdfdstatcl),	FIDENOFFS(mcmdfdstatcl),
		      RSTATOFFS(mrspdirglen),	FIDENOFFS(mrspdirglen)   },
    { MMCMDdirread,   SPDULENOF(mcmdfdread),	FIDENOFFS(mcmdfdread),
		      RSTATOFFS(mrspfdstat),	FIDENOFFS(mrspfdstat)    },
    { MMCMDdirclose,  SPDULENOF(mcmdfdstatcl),	FIDENOFFS(mcmdfdstatcl),
		      NOFFS,			FIDENOFFS(mrspfdclose)   },
    { MMCMDfileopen,  SPDULENOF(mcmdfdopen),	NOFFS,
		      RSTATOFFS(mrspfdopen),	FIDENOFFS(mrspfdopen)    },
    { MMCMDfilestat,  SPDULENOF(mcmdfdstatcl),	FIDENOFFS(mcmdfdstatcl),
		      RSTATOFFS(mrspfdstat),	FIDENOFFS(mrspfdstat)    },
    { MMCMDfilewrite, SPDULENOF(mcmdfilewrite), FIDENOFFS(mcmdfilewrite),
		      RSTATOFFS(mrspfilewrite), FIDENOFFS(mrspfilewrite) },
    { MMCMDfileread,  SPDULENOF(mcmdfdread),	FIDENOFFS(mcmdfdread),
		      RSTATOFFS(mrspfileread),	FIDENOFFS(mrspfileread)  },
    { MMCMDfileclose, SPDULENOF(mcmdfdstatcl),	FIDENOFFS(mcmdfdstatcl),
		      NOFFS,			FIDENOFFS(mrspfdclose),  },
    { MMCMDfiledel,   SPDULENOF(mcmdfiledel),	NOFFS,
		      RSTATOFFS(mrspstat),	NOFFS			 },
    {	0,		0,			0,
			0,			0			 }
};

/* init MMCard command PDU
*  -----------------------
*	lookup MMCard PDU description by command code
*	allocate MMCard PDU buffer plus optionally variable length
*	setup MMCard command code and return allocated PDU length
*/
static int				/* init variable length PDU	*/
_initvpdu( uchar cmd, size_t varlen, union mmcpdu ** ppdu )
{
    struct mpdudesc *	pmpdu;
    union mmcpdu *	spdu;
    size_t		pdulen;

    for ( pmpdu = mmcpdutab; pmpdu->slen != 0; ++pmpdu )
	if ( pmpdu->cmd == cmd ) {
	    pdulen = pmpdu->slen + varlen;
	    if ( ppdu == NULL			/* allocate PDU buffer	*/
	      || (spdu = calloc( 1, pdulen )) == NULL )
		break;
	    spdu->cmd.cmd = cmd;		/* MMCard command code	*/
	    *ppdu = spdu;
	    return pdulen;			/* length of PDU buffer */
	}
    return 0;
}
static int				/* init fixe length PDU 	*/
_initfpdu( uchar cmd, union mmcpdu ** ppdu )
{
    return _initvpdu( cmd, 0, ppdu );
}

/* send MMCard command PDU, receive response PDU
*  ---------------------------------------------
*	optionally encode FILE_IDEN into command PDU
*	send command PDU and release it
*	receive response PDU, MMCrecv() will allocate it
*	optionally decode FILE_IDEN and status from response PDU
*/
static long
_sendrecv( union mmcpdu ** ppdu, size_t * ppdulen, uchar * fdstat )
{
    struct mpdudesc *	pmpdu;
    int 		rval;

    for ( pmpdu = mmcpdutab; pmpdu->cmd != (*ppdu)->cmd.cmd; ++pmpdu )
	if ( pmpdu->slen == 0 )
	    return ERR;			    /* PDU description not in table */

    if ( pmpdu->sidenoffs != NOFFS && fdstat )		/* encode FILE_IDEN */
	putbtxt( (*ppdu)->cmd.data + pmpdu->sidenoffs, fdstat, FIDENSZ );
    rval = MMCsend( (*ppdu)->cmd.data, *ppdulen );	/* send command PDU  */
    free( *ppdu ); *ppdu = NULL; *ppdulen = 0;		/*  and release it   */
    if ( rval < 0 )					/*  ERR, send failed */
	return rval;

    if ( (rval = MMCrecv( (uchar**)ppdu )) < 0 )	/* recv response PDU */
	return rval;					/*  ERR, recv failed */
    *ppdulen = rval;					/* recv'd PDU length */
    if ( pmpdu->ridenoffs != NOFFS && fdstat )		/* decode FILE_IDEN */
	getbtxt( (*ppdu)->rsp.data + pmpdu->ridenoffs, fdstat, FIDENSZ );
    if ( pmpdu->rstatoffs != NOFFS )			/* return status .. */
	return getword( (uchar*)*ppdu + pmpdu->rstatoffs );/* from resp PDU */
    else
	return 0x0000;					   /* or no status  */
}

/* convert FILE_INFO
*  -----------------
*	converts integer fields from IC35's to host's byte order
*/
static void
_cvtfinfo( FILE_INFO * pdudirent, FILE_INFO * hostdirent )
{
    if ( pdudirent == NULL || hostdirent == NULL )
	return;
    getbtxt( (uchar*)pdudirent, (uchar*)hostdirent, sizeof(*hostdirent) );
    hostdirent->ModifyTime = getword( (uchar*)&pdudirent->ModifyTime);
    hostdirent->ModifyDate = getword( (uchar*)&pdudirent->ModifyDate);
    hostdirent->FileSize  = getdword( (uchar*)&pdudirent->FileSize );
}


/* MMCard general commands */
/* ======================= */

/* get MMCard status
*  -----------------
*	IC35 SDK API 13.1  mInitialCard()
*	command:   20  "MMCard" [mmc_num]  00
*	response:  01 00	MMCard present
*		   FF FF	MMCard not detected
*/
long
MMCgetstatus( int mmcnum )
{
    union mmcpdu *	pdu;
    int 		pdulen;
    long		mstat;

    if ( (pdulen = _initfpdu( MMCMDgetstatus, &pdu )) <= 0 )
	return ERR;

    puttext( pdu->cmd.status.mmcard, "MMCard" );
    putbyte( pdu->cmd.status.mmcnum, (char)('0'+mmcnum) );

    if ( (mstat = _sendrecv( &pdu, &pdulen, NULL )) < 0 )
	return mstat;

    free( pdu );
    return mstat;
}

/* get MMCard label
*  ----------------
*	IC35 ADK API 13.4  mGetCardLabel()
*	command:   34  "MMCard" [mmc_num]  00
*	response:  01 00  00  label[8]  00 20 20 00 00 00 00 00 00 00 00 48
*/
long
MMCgetlabel( int mmcnum,  char * plabel )
{
    union mmcpdu *	pdu;
    int 		pdulen;
    long		mstat;

    if ( (pdulen = _initfpdu( MMCMDgetlabel, &pdu )) <= 0 )
	return ERR;

    /* encode cmdargs */
    puttext( pdu->cmd.status.mmcard, "MMCard" );
    putbyte( pdu->cmd.status.mmcnum, (char)('0'+mmcnum) );

    if ( (mstat = _sendrecv( &pdu, &pdulen, NULL )) < 0 )
	return mstat;

    /* decode rspargs */
    gettext( pdu->rsp.label.label, plabel, strlen(pdu->rsp.label.label) );

    free( pdu );
    return mstat;
}


/* MMCard directory commands */
/* ========================= */

/* open MMC directory
*  ------------------
*	IC35 SDK API 13.14  mOpenDirectory()
*	command:   2A  01 00  [MMCard1\path\to\dir]  00
*	response:  01 00  fdiden[27]
*/
long
MMCdiropen( char * dirpath,  uchar * fdstat )
{
    union mmcpdu *	pdu;
    int 		pdulen;
    long		mstat;

    if ( (pdulen = _initvpdu( MMCMDdiropen, strlen(dirpath), &pdu )) <= 0 )
	return ERR;

    /* encode cmdargs */
    putword( pdu->cmd.diropen.mode, 0x0001 );
    puttxt0( pdu->cmd.diropen.path, dirpath );

    if ( (mstat = _sendrecv( &pdu, &pdulen, fdstat )) < 0 )
	return mstat;

    free( pdu );
    return mstat;
}
long
MMCdircreate( char * dirpath,  uchar * fdstat )
{
    union mmcpdu *	pdu;
    int 		pdulen;
    long		mstat;

    if ( (pdulen = _initvpdu( MMCMDdiropen, strlen(dirpath), &pdu )) <= 0 )
	return ERR;

    /* encode cmdargs */
    putword( pdu->cmd.diropen.mode, 0x0000 );
    puttxt0( pdu->cmd.diropen.path, dirpath );

    if ( (mstat = _sendrecv( &pdu, &pdulen, fdstat )) < 0 )
	return mstat;

    free( pdu );
    return mstat;
}

/* get num.of entries in MMCard directory
*  --------------------------------------
*	IC35 ADK API 13.15  mGetDirectorySubItemNum()
*	command:   2B  fdiden[27] 00
*	response:  fdiden[27]  01 00  nn_nn
*/
long
MMCdirgetlen( uchar * fdstat,  ushort * pndent )
{
    union mmcpdu *	pdu;
    int 		pdulen;
    long		mstat;

    if ( (pdulen = _initfpdu( MMCMDdirgetlen, &pdu )) <= 0 )
	return ERR;

    if ( (mstat = _sendrecv( &pdu, &pdulen, fdstat )) < 0 )
	return mstat;

    /* decode rspargs */
    *pndent = getword( pdu->rsp.dirglen.ndent );

    free( pdu );
    return mstat;
}

/* read MMC directory
*  ------------------
*	IC35 SDK API 13.16  mGetDirectorySubItem()
*	command:   2C  fdiden[27]  in_dx 00
*	response:  fdiden[27]  01 00
*		   filename 00 ext 00 at ti_me_st_mp 00 00 fi_le_si_ze
*/
long
MMCdirread( uchar * fdstat, ushort index,  FILE_INFO * pdirent )
{
    union mmcpdu *	pdu;
    int 		pdulen;
    long		mstat;

    if ( (pdulen = _initfpdu( MMCMDdirread, &pdu )) <= 0 )
	return ERR;

    /* encode cmdargs */
    putword( pdu->cmd.dirread.u.index, index );

    if ( (mstat = _sendrecv( &pdu, &pdulen, fdstat )) < 0 )
	return mstat;

    /* decode rspargs */
    _cvtfinfo( &pdu->rsp.dirread.dirent, pdirent );

    free( pdu );
    return mstat;
}

/* close MMC directory
*  -------------------
*	IC35 SDK API 13.18  mCloseDirectory()
*	command:   2E  fdiden[27] 00
*	response:  fdiden[27]
*/
long
MMCdirclose( uchar * fdstat )
{
    union mmcpdu *	pdu;
    int 		pdulen;
    long		mstat;

    if ( (pdulen = _initfpdu( MMCMDdirclose, &pdu )) <= 0 )
	return ERR;

    if ( (mstat = _sendrecv( &pdu, &pdulen, fdstat )) < 0 )
	return mstat;

    free( pdu );
    return mstat;
}


/* MMCard file commands */
/* ==================== */

/* delete MMC file
*  ---------------
*	IC35 SDK API 13.13  mDeleteFile()
*	command:   28  [MMCard1\path\to\file]  00
*	response:  01 00
*/
long
MMCfiledel( char * filepath )
{
    union mmcpdu *	pdu;
    int 		pdulen;
    long		mstat;

    if ( (pdulen = _initvpdu( MMCMDfiledel, strlen(filepath), &pdu )) <= 0 )
	return ERR;

    /* encode cmdargs */
    puttxt0( pdu->cmd.filedel.path, filepath );

    if ( (mstat = _sendrecv( &pdu, &pdulen, NULL )) < 0 )
	return mstat;

    free( pdu );
    return mstat;
}

/* open MMC file
*  -------------
*	IC35 SDK API 13.6  mOpenFile()
*	command:   22  01 00  [MMCard1\path\to\file]  00
*	response:  01 00  fdiden[27]
*/
long
MMCfileopen( char * filepath, ushort mode,  uchar * fdstat, ulong * psize )
{
    union mmcpdu *	pdu;
    int 		pdulen;
    long		mstat;

    if ( (pdulen = _initvpdu( MMCMDfileopen, strlen(filepath), &pdu )) <= 0 )
	return ERR;

    /* encode cmdargs */
    putword( pdu->cmd.fileopen.mode, mode );
    puttxt0( pdu->cmd.fileopen.path, filepath );

    if ( (mstat = _sendrecv( &pdu, &pdulen, fdstat )) < 0 )
	return mstat;

    /* decode rspargs */
    *psize = getdword( pdu->rsp.fileopen.iden+14 );

    free( pdu );
    return mstat;
}

/* get MMC file status
*  -------------------
*	IC35 SDK API 13.10  mGetFileInfo()
*	command:   26  fdiden[27] 00
*	response:  fdiden[27]  01 00
*		   filename 00 ext 00 at ti_me_st_mp 00 00 fi_le_si_ze
*/
long
MMCfilestat( uchar * fdstat,  FILE_INFO * pdirent )
{
    union mmcpdu *	pdu;
    int 		pdulen;
    long		mstat;

    if ( (pdulen = _initfpdu( MMCMDfilestat, &pdu )) <= 0 )
	return ERR;

    if ( (mstat = _sendrecv( &pdu, &pdulen, fdstat )) < 0 )
	return mstat;

    /* decode rspargs */
    _cvtfinfo( &pdu->rsp.filestat.dirent, pdirent );

    free( pdu );
    return mstat;
}

/* read data from MMC file
*  -----------------------
*	IC35 SDK API 13.9  mReadFormFile()
*	command:   24  fdiden[27]  nn_nn  00
*	response:  fdiden[27]  01 00  rr_rr
*		   <rr_rr bytes filedata>
*/
long
MMCfileread( uchar * fdstat, uchar * buff, ushort blen,  ushort * prlen )
{
    union mmcpdu *	pdu;
    int 		pdulen;
    long		mstat;
    size_t		dlen;

    if ( (pdulen = _initfpdu( MMCMDfileread, &pdu )) <= 0 )
	return ERR;

    /* encode cmdargs */
    putword( pdu->cmd.fileread.u.blen, blen );

    if ( (mstat = _sendrecv( &pdu, &pdulen, fdstat )) < 0 )
	return mstat;

    /* decode rspargs */
    *prlen = getword( pdu->rsp.fileread.rlen );
    dlen = pdulen - offsetof(struct mrspfileread, data);
    getbtxt( pdu->rsp.fileread.data, buff, dlen );
    if ( dlen != *prlen )
	LPRINTF(( L_WARN, "MMCfileread: rlen=%u, dlen=%u", *prlen,  dlen ));

    free( pdu );
    return mstat;
}

/* write data to MMC file
*  ----------------------
*	IC35 SDK API 13.8  mWriteToFile()
*	command:   23  fdiden[27]  ww_ww
*		   <ww_ww bytes filedata>  00
*	response:  fdiden[27]  01 00
*/
long
MMCfilewrite( uchar * fdstat, uchar * data, ushort dlen )
{
    union mmcpdu *	pdu;
    int 		pdulen;
    long		mstat;

    if ( (pdulen = _initvpdu( MMCMDfilewrite, dlen, &pdu )) <= 0 )
	return ERR;

    /* encode cmdargs */
    putword( pdu->cmd.filewrite.wlen, dlen );
    putbtxt( pdu->cmd.filewrite.data, data, dlen );
    putbyte( pdu->cmd.filewrite.data+dlen, 0x00 );

    if ( (mstat = _sendrecv( &pdu, &pdulen, fdstat )) < 0 )
	return mstat;

    free( pdu );
    return mstat;
}

/* close MMC file
*  --------------
*	IC35 SDK API 13.11  mCloseFile()
*	command:   27  fdiden[27] 00
*	response:  fdiden[27]
*/
long
MMCfileclose( uchar * fdstat )
{
    union mmcpdu *	pdu;
    int 		pdulen;
    long		mstat;

    if ( (pdulen = _initfpdu( MMCMDfileclose, &pdu )) <= 0 )
	return ERR;

    if ( (mstat = _sendrecv( &pdu, &pdulen, fdstat )) < 0 )
	return mstat;

    free( pdu );
    return mstat;
}
