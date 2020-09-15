/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*/
	static char rcsid[] =
	"$Id: syntrans.c,v 1.19 2001/03/02 02:09:59 tsch Rel $";  	/*
*									*
* IC35 synchronize transactions						*
*									*
*************************************************************************
*									*
*	??? is "fixme" mark: sections of code needing fixes		*
*									*
************************************************************************/

#include <stdio.h>	/* fprintf(), ..	*/
#include <string.h>	/* memcpy(), strlen() ..*/
#include <stdlib.h>	/* calloc(), ..		*/
#include <sys/types.h>	/* size_t, ..		*/

#include "util.h"	/* ERR,OK, uchar, ..	*/
#include "comio.h"	/* com_init(), ..	*/
#include "genproto.h"	/* welcome()		*/
#include "synproto.h"	/* Level-4 commands, ..	*/
#include "syntrans.h"
#include "ic35frec.h"	/* IC35REC, ..		*/
NOTUSED(rcsid);


/* ==================================== */
/*	comunication phases		*/
/* ==================================== */

/* welcome phase
*  -------------
*	opens comm.device and does welcome handshake
*/
static int
synwelcome( char * devname )
{
    int 	rval;

    LPRINTF(( L_INFO, "welcome(%s) ..", devname ));
    if ( com_init( devname ) != OK ) {
	error( "welcome failed: com_init(%s) failed", devname );
	return ERR;
    }
    message( "welcome, start IC35 !" );
    if ( (rval = welcome( 0x41 )) != OK ) {
	error( "welcome %s", rval == ERR_intr ? "aborted"
					      : "failed: no response" );
	return ERR;
    }
    message( "connected" );
    LPRINTF(( L_INFO, "welcome(%s) OK", devname ));
    return OK;
}

/* identify
*  --------
*/
static int
identify( char * rtext )
{
    char	idtext[39+1];

    LPRINTF(( L_INFO, "identify .." ));
    if ( sendcmd( CMDident ) < 0
      || recvrsp( CMDident, idtext, sizeof(idtext) ) < 0 ) {
	error( "identify failed" );
	return ERR;
    }
    if ( rtext != NULL )
	strncat( strcpy( rtext, "" ), idtext, sizeof(idtext)-1 );
    LPRINTF(( L_INFO, "identify OK" ));
    return OK;
}

/* power
*  -----
*/
static int
power( void )
{
    ushort	pstate = 0xFFFF;

    LPRINTF(( L_INFO, "power .." ));
    if ( sendcmd( CMDgetpower ) < 0
      || recvrsp( CMDgetpower, &pstate ) < 0
      || pstate != RSPpowerOK ) {
	error( "power failed (%04X)", pstate );
	return ERR;
    }
    LPRINTF(( L_INFO, "power OK (%04X)", pstate ));
    return OK;
}

/* authenticate
*  ------------
*/
static int
authenticate( char * passwd )
{
    ushort	pstate = 0xFFFF;

    LPRINTF(( L_INFO, "password .." ));
    if ( sendcmd( CMDpassword, passwd ) < 0
      || recvrsp( CMDpassword, &pstate ) < 0
      || pstate != RSPpasswdOK ) {
	error( "password failed (%04X)", pstate );
	return ERR;
    }
    LPRINTF(( L_INFO, "password OK (%04X)", pstate ));
    return OK;
}

/* read/write IC35 reference info
*  ------------------------------
*	reference info can be an arbitrary string of up to 16 chars.
*	IC35sync/Windows uses mmddyyyyhhmmss to retrieve date+time of
*	last sync from IC35 or store it into IC35.
*/
int
ReadSysInfo( char * infobuff )
{
    char	rinfo[16+1];

    LPRINTF(( L_INFO, "ReadSysInfo .." ));
    if ( sendcmd( CMDgetdtime ) < 0
      || recvrsp( CMDgetdtime, rinfo, sizeof(rinfo) ) < 0 ) {
	error( "ReadSysInfo failed" );
	return ERR;
    }
    if ( infobuff != NULL )
	strncat( strcpy( infobuff, "" ), rinfo, sizeof(rinfo)-1 );
    LPRINTF(( L_INFO, "ReadSysInfo OK (%s)", rinfo ));
    return OK;
}
int
WriteSysInfo( char * infodata )
{
    LPRINTF(( L_INFO, "WriteSysInfo %s ..", infodata ));
    if ( sendcmd( CMDsetdtime, infodata ) < 0
      || recvrsp( CMDsetdtime ) < 0 ) {
	error( "WriteSysInfo \"%s\" failed", infodata );
	return ERR;
    }
    message( "set sysinfo \"%s\"", infodata );
    LPRINTF(( L_INFO, "WriteSysInfo OK" ));
    return OK;
}

/* category
*  --------
* ???	semantics not clear
*/
int
category( char * name )
{
    ushort	state = 0xFFFF;

    LPRINTF(( L_INFO, "category(%s) ..", name ));
    if ( sendcmd( CMDcategory, name ) < 0
      || recvrsp( CMDcategory, &state ) < 0 ) {
	error( "category(%s) failed (%04hX)", name, state );
	return ERR;
    }
    LPRINTF(( L_INFO, "category(%s) OK (%04hX)", name, state ));
    return OK;
}

/* open com-device and connect with IC35
*  -------------------------------------
*/
int
connect( char * devname, char * passwd, char * rdtime )
{
    int 	rval;
    char	idtext[64];

    if ( (rval = synwelcome( devname )) == OK
      && (rval = identify( idtext )) == OK
      && (rval = power()) == OK
      && (rval = authenticate( passwd )) == OK
      && (rval = ReadSysInfo( rdtime )) == OK )
	if ( rdtime != NULL )
	    message( "version \"%s\" sysinfo \"%s\"", idtext, rdtime );
	else
	    message( "version \"%s\"", idtext );
    else
	disconnect();
    return rval;
}

/* disconnect from IC35 and close com-device
*  -----------------------------------------
*/
int
disconnect( void )
{
    message( "disconnect" );
    sendcmd( CMDdisconn );
    recvrsp( CMDdisconn );
    com_exit();
    LPRINTF(( L_INFO, "disconnect done." ));
    return OK;
}


/* ==================================== */
/*	access file on IC35		*/
/* ==================================== */

/* open file
*  ---------
*/
int
open_file( char * fname )
{
    ushort	fd;

    LPRINTF(( L_INFO, "open_file(%s) ..", fname ));
    if ( sendcmd( CMDfopen, fname ) < 0
      || recvrsp( CMDfopen, &fd ) < 0 ) {
	error( "openfile(%s) failed", fname );
	return ERR;
    }
    LPRINTF(( L_INFO, "open_file(%s) fd=%04hX", fname, fd ));
    return fd;
}

/* get filelength
*  --------------
*/
/* get total number of records */
int
get_flen( int fd )
{
    ushort	flen;

    LPRINTF(( L_INFO, "get_flen(fd=%04hX) ..", fd ));
    if ( sendcmd( CMDfgetlen, fd ) < 0
      || recvrsp( CMDfgetlen, &flen ) < 0 ) {
	error( "get_flen(fd=%04X) failed", fd );
	return ERR;
    }
    LPRINTF(( L_INFO, "get_flen(fd=%04X) %hd records", fd, flen ));
    return flen;
}
/* get number of modified records */
int
get_mod_flen( int fd )
{
    ushort	flen;

    LPRINTF(( L_INFO, "get_mod_flen(fd=%04X) ..", fd ));
    if ( sendcmd( CMDfgetmlen, fd ) < 0
      || recvrsp( CMDfgetmlen, &flen ) < 0 ) {
	error( "get_mod_flen(fd=%04X) failed", fd );
	return ERR;
    }
    LPRINTF(( L_INFO, "get_mod_flen(fd=%04X) %hd records", fd, flen ));
    return flen;
}

/* read file record
*  ----------------
*	there are 3 methods to read records from IC35 file:
*	- read_frec	read record by index
*	- read_mod_frec	read next modified record
*	- read_id_frec	read record by record-ID
*   return:
*	-1	ERR, communication error
*	0	uncommitted deleted record (read_mod_frec)
*		or record does not exist (read_id_frec)
*	>0	OK, length of record data
*/
static int
_read_frec( IC35REC * rec )
{
    ulong	rid;		/* record-id on IC35	*/
    uchar	chg;		/* change-flag on IC35	*/
    bool	last;		/* last of multi-block	*/
    uchar *	buff;
    size_t	blen;
    uchar	*rptr, *rend;
    int 	rlen;

    if ( (buff = calloc( blen = MAXRLEN, sizeof(*buff) )) == NULL ) {
	error( "_read_frec() failed: no memory for buffer (%d)", blen );
	return -1;
    }
    rend = (rptr = buff) + blen;
    for ( ; ; ) {
	if ( rptr == buff )
	    rlen = recvrsp( CMDfgetrec, &last, &rid, &chg, rptr, rend - rptr );
	else
	    rlen = recvrsp( CMDfgetmore, &last, rptr, rend - rptr );
	if ( rlen < 0 ) {
	    free( buff );
	    error( "_read_frec() failed: recvrsp(%s)",
				rptr == buff ? "getrec" : "getmore" );
	    return ERR;
	}
	rptr += rptr + rlen <= rend ? rlen : 0;
	if ( last )
	    break;
	sendcmd( CMDfgetmore );
    }
    rlen = rptr - buff;
    set_ic35recid( rec, rid );
    set_ic35recchg( rec, chg );
    set_ic35recdata( rec, buff, rlen );
    free( buff );
    return rlen;
}
/* read file record by record-ID */
int
read_id_frec( int fd, ulong rid, IC35REC * rec )
{
    int 	rlen;

    LPRINTF(( L_INFO, "read_id_frec(fd=%04X,rid=%08lX) ..", fd, rid ));
    if ( sendcmd( CMDfgetirec, fd, rid ) < 0 ) {
	error( "read_id_frec(fd=%04X,rid=%08lX) failed: sendcmd", fd, rid );
	return ERR;
    }
    if ( (rlen = _read_frec( rec )) < 0 ) {
	error( "read_id_frec(fd=%04X,rid=%08lX) failed: recvrsp", fd, rid );
	return ERR;
    }
    if ( ic35recid( rec ) != rid ) {
	LPRINTF(( L_INFO, "read_id_frec(fd=%04X,rid=%08lX) non-exist", fd, rid ));
	return 0;
    }
    LPRINTF(( L_INFO, "read_id_frec(fd=%04X,rid=%08lX) len=%d", fd, rid, rlen ));
    return rlen;
}
/* read file record by index	*/
int
read_frec( int fd, int idx, IC35REC * rec )
{
    int 	rlen;

    LPRINTF(( L_INFO, "read_frec(fd=%04X,i=%04X) ..", fd, idx ));
    if ( sendcmd( CMDfgetrec, fd, idx ) < 0 ) {
	error( "read_frec(fd=%04X,i=%04X) failed: sendcmd", fd, idx );
	return ERR;
    }
    if ( (rlen = _read_frec( rec )) < 0 ) {
	error( "read_frec(fd=%04X,i=%04X) failed: recvrsp", fd, idx );
	return ERR;
    }
    LPRINTF(( L_INFO, "read_frec(fd=%04X,i=%04X) len=%d", fd, idx, rlen ));
    return rlen;
}
/* read next modified file record */
int
read_mod_frec( int fd, IC35REC * rec )
{
    int 	rlen;

    LPRINTF(( L_INFO, "read_mod_frec(fd=%04X) ..", fd ));
    if ( sendcmd( CMDfgetmrec, fd ) < 0 ) {
	error( "read_mod_frec(fd=%04X) failed: sendcmd", fd );
	return ERR;
    }
    if ( (rlen = _read_frec( rec )) < 0 ) {
	error( "read_mod_frec(fd=%04X) failed: recvrsp", fd );
	return ERR;
    }
    LPRINTF(( L_INFO, "read_mod_frec(fd=%04X) len=%d", fd, rlen ));
    return rlen;
}

/* write/update file record
*  ------------------------
*	write_frec() writes new record, which gets new record-ID,
*	update_frec() writes new data into an existing record and
*	keeps same record-ID.
* ???	the meaning of magic is unclear, perceived were:
*	IC35file	bytes
*	Addresses	51 00 00
*	Memo		58 48 99
*			60 16 99
*	Schedule	10 00 00
*	ToDoList	10 00 00
*/
#define DOwrite 	TRUE
#define DOupdate	FALSE

static int
_write_frec( bool do_write, int fd, IC35REC * rec )
{
    uchar *	data;
    size_t	dlen;
    uchar *	wptr;
    size_t	wlen;
    uchar *	magic;
    bool	last;
    int 	rval;
    ulong	orecid, recid;

    orecid = ic35recid( rec );
    get_ic35recdata( rec, &data, &dlen );
    switch ( FileId( orecid ) ) {
    case FILEADDR:  magic = "\x51\x00\x00"; break;
    case FILEMEMO:  magic = "\x60\x16\x99"; break;
    case FILETODO:
    case FILESCHED: magic = "\x10\x00\x00"; break;
    default:        magic = "\x00\x00\x00"; break;
    }
    wptr = data; 
    do {
	wlen = data+dlen - wptr;
	if ( wlen > 80 ) wlen = 80;
	last = (bool)( wptr+wlen >= data+dlen );
	if ( wptr == data )
	  if ( do_write )
	    rval = sendcmd( CMDfputrec, last, fd, 0L,     magic, dlen,
					       wptr, wlen );
	  else /* update */
	    rval = sendcmd( CMDfupdrec, last, fd, orecid, magic, dlen,
					       wptr, wlen );
	else
	    rval = sendcmd( CMDfputmore, last, wptr, wlen );
	if ( rval < 0 )
	    return -1;
	if ( recvrsp( CMDfputrec, &recid ) < 0 )
	    return -2;
	wptr += wlen;
    } while ( wptr < data+dlen );
    set_ic35recid( rec, recid );
    return 0;
}
/* write record, gets new record-ID */
int
write_frec( int fd, IC35REC * rec )
{
    int 	rval;

    LPRINTF(( L_INFO, "write_frec(fd=%04X) ..", fd ));
    if ( (rval = _write_frec( DOwrite, fd, rec )) < 0 ) {
	error( "write_frec(fd=%04X) failed: %s",
			    fd, rval == -1 ? "sendcmd" : "recvrsp" );
	return ERR;
    }
    LPRINTF(( L_INFO, "write_frec(fd=%04X) rid=%08lX", fd, ic35recid( rec ) ));
    return OK;
}
/* write record update, keeps record-ID */
int
update_frec( int fd, IC35REC * rec )
{
    int 	rval;
    ulong	orecid;

    orecid = ic35recid( rec );
    LPRINTF(( L_INFO, "update_frec(fd=%04X,orid=%08lX) ..", fd, orecid ));
    if ( (rval = _write_frec( DOupdate, fd, rec )) < 0 ) {
	error( "update_frec(fd=%04X,orid=%08lX) failed: %s",
			    fd, orecid, rval == -1 ? "sendcmd" : "recvrsp" );
	return ERR;
    }
    LPRINTF(( L_INFO, "update_frec(fd=%04X,orid=%08lX) nrid=%08lX",
				  fd, orecid, ic35recid( rec ) ));
    return OK;
}

/* delete file record
*  ------------------
*/
int
delete_frec( int fd, ulong recid )
{
    LPRINTF(( L_INFO, "delete_frec(fd=%04X,rid=%08lX) ..", fd, recid ));
    if ( sendcmd( CMDfdelrec, fd, recid ) < 0
      || recvrsp( CMDfdelrec ) < 0 ) {
	error( "delete_frec(fd=%04X,rid=%08X) failed", fd, recid );
	return ERR;
    }
    LPRINTF(( L_INFO, "delete_frec(fd=%04X,rid=%08lX) OK", fd, recid ));
    return OK;
}

/* commit file record, i.e. reset change flag
*  ------------------------------------------
*/
int
commit_frec( int fd, ulong recid )
{
    LPRINTF(( L_INFO, "commit_frec(fd=%04X,rid=%08lX) ..", fd, recid ));
    if ( sendcmd( CMDfclrchg, fd, recid ) < 0
      || recvrsp( CMDfclrchg ) < 0 ) {
	error( "commit_frec(fd=%04hX,rid=%08X) failed", fd, recid );
	return ERR;
    }
    LPRINTF(( L_INFO, "commit_frec(fd=%04hX,rid=%08lX) OK", fd, recid ));
    return OK;
}

/* close file
*  ----------
*/
int
close_file( int fd )
{
    LPRINTF(( L_INFO, "close_file(fd=%04X) ..", fd ));
    if ( sendcmd( CMDfclose, fd ) < 0
      || recvrsp( CMDfclose ) < 0 ) {
	error( "close_file(fd=%04X) failed", fd );
	return ERR;
    }
    LPRINTF(( L_INFO, "close_file(fd=%04X) OK", fd ));
    return OK;
}
