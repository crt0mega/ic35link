/************************************************************************
* Copyright (C) 2001 Thomas Schulz					*
*									*/
	static char rcsid[] =
	"$Id: mgrtrans.c,v 1.17 2001/11/20 23:08:35 thosch Exp $";  	/*
*									*
* IC35 manager transactions						*
*									*
*************************************************************************
*									*
*	??? is "fixme" mark: sections of code needing fixes		*
*
*		communication phases
*	mconnect
*	mdisconnect
*		MMCard info
*	mmc_status
*	mmc_label
*		MMCard directory ops
*	mmc_opendir
*       mmc_createdir
*	mmc_readdir
*	mmc_closedir
*	mmctstampstr
*	mmctstampunixtime
*		MMCard file operations
*	mmc_delfile
*	mmc_openfile
*	mmc_statfile
*	mmc_readfile
*	mmc_writefile
*	mmc_closefile
*		IC35 database backup,restore
*	readdatabase
*	writedatabase
*									*
************************************************************************/

#include <stdio.h>	/* fprintf(), ..	*/
#include <string.h>	/* memcpy(), strlen() ..*/
#include <stdlib.h>	/* malloc(), ..		*/
#include <unistd.h>	/* usleep()		*/
#include <sys/types.h>	/* size_t, ..		*/
#include <time.h>	/* time_t, localtime()..*/
#include <errno.h>

#include "util.h"	/* ERR,OK, uchar, ..	*/
#include "comio.h"	/* com_init(), ..	*/
#include "genproto.h"	/* welcome()		*/
#include "mgrproto.h"	/* manager protocol	*/
#include "mgrtrans.h"
NOTUSED(rcsid);


/* ==================================== */
/*	communication phases		*/
/* ==================================== */

/* welcome phase
*  -------------
*	opens comm.device and does welcome handshake
*/
static int
mgrwelcome( char * devname )
{
    int 	rval;

    LPRINTF(( L_INFO, "welcome(%s) ..", devname ));
    if ( com_init( devname ) != OK ) {
	error( "welcome failed: com_init(%s) failed", devname );
	return ERR;
    }
    message( "welcome, start IC35 !" );
    if ( (rval = welcome( 0x40 )) != OK ) {
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
*	communication PC <-> IC35:
*	reset-phase	-> 09
*			<- 90
*	ident-phase	-> 10
*			<- 90 "DCS_SDK" 00
*/
static int
identify( char rtext[7+1] )
{
    int 	rlen;
    char	rbuff[7+1];	/* for "DCS_SDK\x00" */

    LPRINTF(( L_INFO, "identify .." ));

    if ( Mcmdrsp( MCMDreset, MRSPgotcmd ) != OK ) {
	error( "identify failed: reset-phase" );
	return ERR;
    }
    if ( Mcmdrsp( MCMDident, MRSPgotcmd ) != OK
      || (rlen = com_recv( rbuff, sizeof(rbuff) )) <= 0 ) {
	error( "identify failed: ident-phase" );
	return ERR;
    }
    if ( rtext != NULL )
	strncat( strcpy( rtext, "" ), rbuff, rlen );

    LPRINTF(( L_INFO, "identify OK" ));
    return OK;
}

/* init IC35, optionally get status data
*  -------------------------------------
*	communication PC <-> IC35:
*	optional:
*	  -> FF
*	  <- [block of 16400 bytes]
*	mandatory:
*	  -> 50
*	  <- 90  or  timeout 0.1 sec
*	IC35 may send response, but anyway needs time to get ready
*/
#define STATSIZE	16400

static int
status( char * fname )
{
    uchar	cmd;
    char *	rbuff;
    int 	rlen = -1;
    FILE *	fp;
    int 	old_tmo;
    uchar	rsp;

    LPRINTF(( L_INFO, "status(%s) ..", fname ? fname : "NULL" ));

    if ( fname && *fname ) {
	if ( (rbuff = malloc( STATSIZE )) != NULL ) {
	    cmd = MCMDstatus;
	    com_send( &cmd, 1 );
	    if ( (rlen = com_recv( rbuff, STATSIZE )) < 0 ) {
		error( "status failed: receive" );
		free( rbuff );
		return ERR;
	    }
	    if ( (fp = fopen( fname, "w")) != NULL ) {
		fwrite( rbuff, 1, rlen, fp );
		fclose( fp );
	    } else {
		message( "cannot open statfile: %s", fname );
	    }
	    free( rbuff );
	} else {
	    message( "cannot read statdata: no memory (%d)", STATSIZE );
	}
    }
    cmd = MCMDinit;
    com_send( &cmd, 1 );		/* -> 50	*/
    old_tmo = com_settimeout( 100 );
    com_recv( &rsp, 1 );		/* <- 90  or  wait 100 msec	*/
    com_settimeout( old_tmo );

    if ( rlen >= 0 )			/* received status block	*/
	LPRINTF(( L_INFO, "status received %d bytes statdata", rlen ));
    else
	LPRINTF(( L_INFO, "status OK" ));
    return OK;
}

/* open com-device and connect with IC35
*  -------------------------------------
*/
int
mconnect( char * devname, char * statfname )
{
    int 	rval;
    char	idtext[8];

    LPRINTF(( L_INFO, "mconnect(%s,%s) ..",
				devname, statfname ? statfname : "NULL" ));
    if ( (rval = mgrwelcome( devname )) == OK
      && (rval = identify( idtext )) == OK ) {
	message( "identity \"%s\"", idtext );
	if ( statfname && *statfname )
	    message( "statdata %s", statfname );
	rval = status( statfname );
    }
    if ( rval != OK )
	mdisconnect();
    LPRINTF(( L_INFO, "mconnect(%s,%s) %s",
			  devname, statfname ? statfname : "NULL",
					rval == OK ? "OK" : "ERR" ));
    return rval;
}

/* disconnect from IC35 and close com-device
*  -----------------------------------------
*	communication PC <-> IC35:
*	-> 09
*	timeout
*	-> 09
*	<- 90
*	-> 01
*	timeout
*	-> 01
*	<- 90
*	until response '\x90' is received, send '\x09' or '\x01'
*	respectively will be retried up to 5 times.
*/
int
mdisconnect( void )
{
    uchar	cmd, rsp;
    int 	rval;

    message( "disconnect" );
    if ( (rval = Mcmdrsp( cmd = MCMDreset, rsp = MRSPgotcmd )) != OK
      || (rval = Mcmdrsp( cmd = MCMDdisconn, rsp = MRSPgotcmd )) != OK )
	message( "disconnect sent '\\x%02X', failed receive '\\x%02X'",
				      cmd,			rsp );
    com_exit();
    LPRINTF(( L_INFO, "disconnect done." ));
    return rval;
}


/* ==================================== */
/*	IC35 MMCard info		*/
/* ==================================== */

/* local: convert mstat to text
*  ----------------------------
*/
static char *
_mstatxt( long mstat )
{
    static char mstatxt[2+11+1];	/* "(-nnnnnnnnnn)"\0 */

    if ( mstat >= 0 )
	sprintf( mstatxt, "(%04hX)", (ushort)mstat );
    else
	sprintf( mstatxt, "(%ld)", mstat );
    return mstatxt;
}

/* get MMCard status
*  -----------------
*   returns:
*	1	OK, MMCard found
*	0	OK, MMCard not detected
*	-1	ERR, communication failed
*/
int
mmc_status( int mmcnum )
{
    long	mstat;

    if ( mmcnum != 1 && mmcnum != 2 ) {
	error( "mmc_status: bad MMCard number %d", mmcnum );
	return ERR;
    }
    LPRINTF(( L_INFO, "mmc_status(%d) ..", mmcnum ));
    if ( (mstat = MMCgetstatus( mmcnum )) < 0
      || !( mstat == 0xFFFF || mstat == 0x0001 ) ) {
	error( "mmc_status failed %s", _mstatxt(mstat) );
	return ERR;
    }
    LPRINTF(( L_INFO, "mmc_status(%d) = %04X", mmcnum, mstat ));
    return mstat == 0xFFFF ? 0 : mstat;
}

/* get MMCard label
*  ----------------
*/
int
mmc_label( int mmcnum, char label[11+1] )
{
    long	mstat;

    if ( mmcnum != 1 && mmcnum != 2 ) {
	error( "mmc_label: bad MMCard number %d", mmcnum );
	return ERR;
    }
    LPRINTF(( L_INFO, "mmc_label(%d) ..", mmcnum ));
    if ( (mstat = MMCgetlabel( mmcnum, label )) != 0x0001 ) {
	error( "mmc_label failed %s", _mstatxt(mstat) );
	return ERR;
    }
    LPRINTF(( L_INFO, "mmc_label(%d) = \"%s\"", mmcnum, label ));
    return OK;
}


/* ==================================== */
/*	IC35 MMCard directory ops	*/
/* ==================================== */

struct mmcdir {			/* MMC directory descriptor	*/
    uchar	fdiden[FIDENSZ];/*  MMC dir/file access ident	*/
    MMCDIRENT	dirent;		/*  decoded directory entry	*/
    ushort	ndirent;	/*  number of directory entries */
    ushort	dirindex;	/*  current directory index	*/
};

/* local: convert FILE_INFO to MMCDIRENT
*  -------------------------------------
*/
static void
_mmc_finfo2dirent( FILE_INFO * finfo, MMCDIRENT * dirent )
{
    if ( finfo == NULL || dirent == NULL )
	return;
    strncat( strcpy( dirent->name, "" ), finfo->FileName, 8 );
    if ( strlen( finfo->ExtName ) != 0 ) {
	strcat(  dirent->name, "." );
	strncat( dirent->name, finfo->ExtName, 3 );
    }
    dirent->attr   = finfo->Attribute;
    dirent->tstamp = finfo->ModifyDate << 16 | finfo->ModifyTime;
    dirent->size   = finfo->FileSize;
}

/* open MMC directory
*  ------------------
*/
MMCDIR *
mmc_opendir( char * dirpath )
{
    MMCDIR *	dirp;
    long	mstat;

    if ( (dirp = malloc( sizeof(*dirp) )) == NULL ) {
	error( "mmc_opendir failed: no memory (%d)", sizeof(*dirp) );
	return NULL;
    }
    LPRINTF(( L_INFO, "mmc_opendir(%s) ..", dirpath ));
    if ( (mstat = MMCdiropen( dirpath, dirp->fdiden )) != 0x0001 ) {
	error( "mmc_opendir(%s) diropen failed %s", dirpath, _mstatxt(mstat) );
	return NULL;
    }
    if ( (mstat = MMCdirgetlen( dirp->fdiden, &dirp->ndirent )) != 0x0001 ) {
	error( "mmc_opendir(%s) dirgetlen failed %s", dirpath,_mstatxt(mstat) );
	return NULL;
    }
    dirp->dirindex = 0;
    LPRINTF(( L_INFO, "mmc_opendir(%s) OK, ndirent=%d",
				  dirpath,        dirp->ndirent ));
    return dirp;
}
/* open MMC directory
*  ------------------
*/
MMCDIR *
mmc_createdir( char * dirpath )
{
    MMCDIR *	dirp;
    long	mstat;

    if ( (dirp = malloc( sizeof(*dirp) )) == NULL ) {
	error( "mmc_createdir failed: no memory (%d)", sizeof(*dirp) );
	return NULL;
    }
    LPRINTF(( L_INFO, "mmc_createdir(%s) ..", dirpath ));
    if ( (mstat = MMCdircreate( dirpath, dirp->fdiden )) != 0x0001 ) {
	error( "mmc_createdir(%s) dircreate failed %s", dirpath, _mstatxt(mstat) );
	return NULL;
    }
    if ( (mstat = MMCdirgetlen( dirp->fdiden, &dirp->ndirent )) != 0x0001 ) {
	error( "mmc_createdir(%s) dirgetlen failed %s", dirpath,_mstatxt(mstat) );
	return NULL;
    }
    dirp->dirindex = 0;
    LPRINTF(( L_INFO, "mmc_createdir(%s) OK, ndirent=%d",
				  dirpath,        dirp->ndirent ));
    return dirp;
}

/* read MMC directory
*  ------------------
*/
MMCDIRENT *
mmc_readdir( MMCDIR * dirp )
{
    long	mstat;
    FILE_INFO	mmcdirent;

    if ( dirp == NULL )
	return NULL;
    if ( dirp->dirindex >= dirp->ndirent ) {
	LPRINTF(( L_INFO, "mmc_readdir: eodir (%d)", dirp->dirindex ));
	return NULL;
    }
    ++dirp->dirindex;
    LPRINTF(( L_INFO, "mmc_readdir index=%d ..", dirp->dirindex ));
    if ( (mstat = MMCdirread( dirp->fdiden, dirp->dirindex, &mmcdirent ))
	  != 0x0001 ) {
	error( "mmc_readdir failed %s", _mstatxt(mstat) );
	return NULL;
    }
    _mmc_finfo2dirent( &mmcdirent, &dirp->dirent );
    LPRINTF(( L_INFO, "mmc_readdir OK" ));
    return &dirp->dirent;
}

/* close MMC directory
*  -------------------
*/
int
mmc_closedir( MMCDIR * dirp )
{
    if ( dirp == NULL )
	return OK;
    LPRINTF(( L_INFO, "mmc_closedir .." ));
    if ( MMCdirclose( dirp->fdiden ) < 0 ) {
	error( "mmc_closedir failed" );
	return ERR;
    }
    free( dirp );
    LPRINTF(( L_INFO, "mmc_closedir OK" ));
    return OK;
}

/* convert MMC timestamp
*  ---------------------
*	the timestamp of MMCard dir/file is encoded like DOS:
*	  3322222 2222 21111 11111 100000 00000   bit- ..
*	  1098765 4321 09876 54321 098765 43210   .. number
*	  yyyyyyy mmmm ddddd hhhhh mmmmmm sssss   bit fields
*	the bit field meanings are:
*	  yyyyyyy	years since 1980
*	  mmmm		month	  1..12
*	  ddddd		day	  1..31
*	  hhhhh		hour	 00..24
*	  mmmmmm	minute	 00..59
*	  sssss		second/2 00..31
*   mmctstampstr() returns:
*	(char*) 	timestamp string: yyyy-mm-dd hh:mm:ss
*   mmctstampunixtime() returns:
*	(time_t)	timestamp converted to Unix time
*/
static struct tm *
_mmctstamptm( ulong tstamp )
{
    static struct tm	mmctm;

    mmctm.tm_year = ((tstamp & 0xFE000000) >> (4+5+5+6+5)) + 80;
    mmctm.tm_mon  = ((tstamp & 0x01E00000) >> (  5+5+6+5)) - 1;
    mmctm.tm_mday =  (tstamp & 0x001F0000) >> (    5+6+5);
    mmctm.tm_hour =  (tstamp & 0x0000F800) >> (      6+5);
    mmctm.tm_min  =  (tstamp & 0x000007E0) >> (        5);
    mmctm.tm_sec  =  (tstamp & 0x0000001F)                 * 2;
    return &mmctm;
}
char *
mmctstampstr( ulong tstamp )
{
    static char ymdhms[19+1];
    struct tm * ptm;

    ptm = _mmctstamptm( tstamp );
    sprintf( ymdhms, "%04u-%02u-%02u %02u:%02u:%02u",
			ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday,
			ptm->tm_hour,      ptm->tm_min,   ptm->tm_sec );
    return ymdhms;
}
time_t
mmctstampunixtime( ulong tstamp )
{
    struct tm * ptm;

    ptm = _mmctstamptm( tstamp );
    ptm->tm_isdst = -1;
    return mktime( ptm );
}


/* ==================================== */
/*	IC35 MMCard file operations	*/
/* ==================================== */

struct mmcfile {		/* MMC file descriptor	*/
    uchar	fdiden[FIDENSZ];/*  MMC dir/file access ident	*/
    MMCDIRENT	filestat;	/*  decoded file status		*/
    ulong	size;		/*  file size			*/
    ulong	offset;		/*  current offset in file	*/
};

/* delete MMC file
*  ---------------
*/
int
mmc_delfile( char * filepath )
{
    long	mstat;

    LPRINTF(( L_INFO, "mmc_delfile(%s) ..", filepath ));
    if ( (mstat = MMCfiledel( filepath )) != 0x0001 ) {
	error( "mmc_delfile(%s) failed %s", filepath, _mstatxt(mstat) );
	return ERR;
    }
    LPRINTF(( L_INFO, "mmc_delfile(%s) OK", filepath ));
    return OK;
}

/* open MMC file
*  -------------
*/
MMCFILE *
mmc_openfile( char * filepath, ushort mode )
{
    MMCFILE *	fp;
    long	mstat;

    if ( (fp = malloc( sizeof(*fp) )) == NULL ) {
	error( "mmc_openfile failed: no memory (%d)", sizeof(*fp) );
	return NULL;
    }
    LPRINTF(( L_INFO, "mmc_openfile(%s) ..", filepath ));
    if ( (mstat = MMCfileopen( filepath, mode, fp->fdiden, &fp->size ))
	  != 0x0001 ) {
	error( "mmc_openfile(%s) failed %s", filepath, _mstatxt(mstat) );
	return NULL;
    }
    fp->offset = 0;
    LPRINTF(( L_INFO, "mmc_openfile(%s) OK, size=%lu", filepath, fp->size ));
    return fp;
}

/* get MMC file status
*  -------------------
*/
MMCDIRENT *
mmc_statfile( MMCFILE * fp )
{
    long	mstat;
    FILE_INFO	mmcdirent;

    if ( fp == NULL )
	return NULL;
    LPRINTF(( L_INFO, "mmc_statfile .." ));
    if ( (mstat = MMCfilestat( fp->fdiden, &mmcdirent )) != 0x0001 ) {
	error( "mmc_statfile failed %s", _mstatxt(mstat) );
	return NULL;
    }
    _mmc_finfo2dirent( &mmcdirent, &fp->filestat );
    LPRINTF(( L_INFO, "mmc_statfile OK" ));
    return &fp->filestat;
}

/* read data from MMC file
*  -----------------------
*/
int
mmc_readfile( MMCFILE * fp, uchar * buff, size_t blen )
{
    long	mstat;
    ushort	rlen;

    if ( fp == NULL )
	return ERR;
    if ( fp->offset >= fp->size ) {
	LPRINTF(( L_INFO, "mmc_readfile: eofile (%d)", fp->offset ));
	return 0;
    }
    LPRINTF(( L_INFO, "mmc_readfile(buff,%d) ..", blen ));
    if ( fp->offset + blen > fp->size )
	blen = fp->size - fp->offset;
    if ( (mstat = MMCfileread( fp->fdiden, buff, blen, &rlen )) != 0x0001 ) {
	error( "mmc_readfile failed %s", _mstatxt(mstat) );
	return ERR;
    }
    fp->offset += rlen;
    LPRINTF(( L_INFO, "mmc_readfile(buff,%d) = %d", blen, rlen ));
    return rlen;
}

/* write data to MMC file
*  ----------------------
*/
int
mmc_writefile( MMCFILE * fp, uchar * data, size_t dlen )
{
    long	mstat;

    if ( fp == NULL )
	return ERR;
    LPRINTF(( L_INFO, "mmc_writefile(data,%d) ..", dlen ));
    if ( (mstat = MMCfilewrite( fp->fdiden, data, dlen )) != 0x0001 ) {
	error( "mmc_writefile failed %s", _mstatxt(mstat) );
	return ERR;
    }
    if ( fp->offset == fp->size )
	fp->size += dlen;
    fp->offset += dlen;
    LPRINTF(( L_INFO, "mmc_writefile(data,%d) OK", dlen ));
    return dlen;
}

/* close MMC file
*  --------------
*/
int
mmc_closefile( MMCFILE * fp )
{
    if ( fp == NULL )
	return OK;
    LPRINTF(( L_INFO, "mmc_closefile .." ));
    if ( MMCfileclose( fp->fdiden ) < 0 ) {
	error( "mmc_closefile failed" );
	return ERR;
    }
    free( fp );
    LPRINTF(( L_INFO, "mmc_closefile OK" ));
    return OK;
}


/* ==================================== */
/*	IC35 database backup,restore	*/
/* ==================================== */

#define HEADBLKSZ	  136
#define DATABLKSZ	16384
#define INFOBLKSZ	    4
#define NDATABLKS	26
#define NINFOBLKS	 2

/* get database info
*  -----------------
*/
static int
_getdbinfo( uchar * buff, size_t blen )
{
    if ( Mcmdrsp( MCMDinfo, MRSPgotcmd ) != OK )
	return ERR;
    return com_recv( buff, blen );
}

/* textify protocol errorcode
*  --------------------------
*/
static char *
_dbetext( int len )
{
    static char	etext[24+1];

    switch ( len ) {
    case ERR_recv:
	return "recv.error/timeout";
    case ERR_acknak:
	return "ack/nak handshake failed";
    case ERR_chksum:
	return "checksum error";
    default:
	sprintf( etext, "%s (%d)", len < 0 ? "error" : "bad length", len );
	return etext;
    }
}

/* IC35 database backup to file
*  ============================
*	communication PC <-> IC35:
*   backup command
*	-> 13				command backup
*	<- 90				response gotcmd
*	on receive error / timeout retry sending command
*   headblock
*	<- [136-byte-block]  cc_cc	headblock
*	-> 60				positive acknowledge
*	<- A0				response gotack
*	  or on checksum mismatch:
*	  -> 62				negative acknowlegde
*	  <- A0				reponse gotnak
*	  and receive headblock again
*   datablocks
*	<- [16384-byte-block]  cc_cc	datablock
*	-> 60				positive acknowledge
*	<- A0				response gotack
*	repeat receive for 26 datablocks of 16384 bytes. for each datablock
*	do retry on checksum mismatch like above for headblock.
*   database info
*	-> 18				command getinfo
*	<- 90				response gotcmd
*	<- 30 31 32 38			infoblock-1
*	-> 18				command getinfo
*	<- 90				response gotcmd
*	<- 30 31 32 38			infoblock-2
*/

/* read IC35 database to file
*  --------------------------
*/
static int
_readdatabase( char * fname, FILE * fp, uchar * buff )
{
    ulong	frlen;
    int 	rlen;
    int 	i;

    LPRINTF(( L_INFO, "readdatabase %s ..", fname ));

		/* send backup command and wait for IC35 response	*/
    if ( Mcmdrsp( MCMDbackup, MRSPgotcmd ) != OK ) {
	error( "readdatabase backup command failed" );
	return ERR;
    }
		/* receive head-,data-blocks from IC35 and write to file */
    frlen = 0;
    for ( i = 0; i <= NDATABLKS; ++i ) {
	char		btext[24+1];
	size_t		blen;
	if ( i == 0 ) {
	    fprintf( stderr, "read database -> %s head  ", fname );
	    strcpy( btext, "headblock" );
	    blen = HEADBLKSZ;
	} else {
	    sprintf( btext, "datablock-%d", i );
	    blen = DATABLKSZ;
	}
	if ( (rlen = Mrecvblk( buff, blen )) != blen ) {
	    fprintf( stderr, "\n" );
	    error( "readdatabase %s %s", btext, _dbetext( rlen ) );
	    return ERR;
	}
	frlen += rlen;
	fprintf( stderr, "\rread database -> %s %ldk  ",
				  fname, (frlen+511)/1024 );
	if ( fwrite( buff, sizeof(buff[0]), rlen, fp ) != rlen ) {
	    fprintf( stderr, "\n" );
	    error( "readdatabase %s %s: write error %s (%d)",
			      btext, fname, strerror(errno), errno );
	    return ERR;
	}
    }
		/* receive database info from IC35 and write to file	*/
    fprintf( stderr, "\nread database -> %s info  ", fname );
    for ( i = 1; i <= NINFOBLKS; ++i ) {
	if ( (rlen = _getdbinfo( buff, INFOBLKSZ )) != INFOBLKSZ ) {
	    fprintf( stderr, "\n" );
	    error( "readdatabase info-%d recv.error/timeout", i );
	    return ERR;
	}
	if ( fwrite( buff, sizeof(buff[0]), rlen, fp ) != rlen ) {
	    fprintf( stderr, "\n" );
	    error( "readdatabase info-%d %s: write error %s (%d)",
			    i, fname, strerror(errno), errno );
	    return ERR;
	}
    }
    fprintf( stderr, "\n" );

    LPRINTF(( L_INFO, "readdatabase %s OK", fname ));
    return OK;
}

/* read IC35 database: init,free resources
*  ---------------------------------------
*/
int
readdatabase( char * fname )
{
    uchar *	buff;
    FILE *	fp;
    int 	rval;

    if ( (buff = malloc( DATABLKSZ )) != NULL ) {
	if ( fname && *fname
	  && (fp = fopen( fname, "w" )) != NULL ) {
	    rval = _readdatabase( fname, fp, buff );
	    fclose( fp );
	} else {
	    error( "readdatabase cannot open %s", fname ? fname : "(NULL)" );
	    rval = ERR;
	}
	free( buff );
    } else {
	error( "readdatabase failed: no memory (%d)", DATABLKSZ );
	rval = ERR;
    }
    return rval;
}


/* IC35 database restore from file
*  ===============================
*	communication PC <-> IC35:
*   database info
*	-> 18				command getinfo
*	<- 90				response gotcmd
*	<- 30 31 32 38			infoblock-1
*	-> 18				command getinfo
*	<- 90				response gotcmd
*	<- 30 31 32 38			infoblock-2
*	check both infoblocks matching with tail of database file,
*	on mismatch abort and do not write database to IC35.
*   restore commands
*	-> 14				command restore-1
*	<- 90				response gotcmd
*	-> 70				command restore-2
*	<- C0				response restore-2
*	on receive error / timeout retry sending command-1/2.
*   headblock
*	-> [136-byte-block]		headblock
*	<- cc_cc			response checksum
*	-> 60				positive acknowledge
*	<- A0				response gotack
*	  or on checksum mismatch:
*	  -> 62				negative acknowlegde
*	  <- A0				reponse gotnak
*	  and send headblock again.
*   datablocks
*	-> [16384-byte-block]		datablock
*	<- cc_cc			response checksum
*	-> 60				positive acknowledge
*	<- A0				response gotack
*	repeat send for 26 datablocks of 16384 bytes. for each datablock do
*	retry on checksum mismatch like above for headblock.
*	after all data is written to IC35, wait 3.25 sec to
*	avoid failure in mdisconnect().
*   warning: IC35 does NOT restore phonetype and date+time,
*	they must be set manually after restore from file !
*/

/* write IC35 database from file
*  -----------------------------
*/
static int
_writedatabase( char * fname, FILE * fp, uchar * buff )
{
    ulong	fwlen;
    int 	wlen;
    int 	i;

    LPRINTF(( L_INFO, "writedatabase %s ..", fname ));

		/* check info from IC35 matching info in database file	*/
    fprintf( stderr, "write database <> %s info  ", fname );
    fseek( fp, -2*INFOBLKSZ, SEEK_END );
    for ( i = 1; i <= NINFOBLKS; ++i ) {
	uchar		info[INFOBLKSZ];
	if ( (wlen = _getdbinfo( buff, INFOBLKSZ )) != INFOBLKSZ ) {
	    fprintf( stderr, "\n" );
	    error( "writedatabase info-%d recv.error/timeout", i );
	    return ERR;
	}
	if ( fread( info, sizeof(info[0]), INFOBLKSZ, fp ) != INFOBLKSZ ) {
	    fprintf( stderr, "\n" );
	    error( "writedatabase info-%d %s: read error %s (%d)",
			    i, fname, strerror(errno), errno );
	    return ERR;
	}
	if ( memcmp( buff, info, INFOBLKSZ ) != 0 ) {
	    char *	hextext;
	    int		j;
	    fprintf( stderr, "\n" );
	    if ( (hextext = malloc( INFOBLKSZ*3 + 1 )) != NULL ) {
		strcpy( hextext, "" );
		for ( j = 0; j < INFOBLKSZ; ++j )
		    sprintf( hextext+strlen(hextext), "%02X ", buff[j] );
		message( "%s  IC35", hextext );
		strcpy( hextext, "" );
		for ( j = 0; j < INFOBLKSZ; ++j )
		    sprintf( hextext+strlen(hextext), "%02X ", info[j] );
		message( "%s  %s", hextext, fname );
		free( hextext );
	    }
	    error( "writedatabase info-%d mismatch", i );
	}
    }
    rewind( fp );

		/* send restore commands and wait for IC35 responses	*/
    fprintf( stderr, "\n" );
    if ( Mcmdrsp( MCMDrestinit, MRSPgotcmd ) < 0
      || Mcmdrsp( MCMDrestdata, MRSPrestdata ) < 0 ) {
	error( "writedatabase restore command failed" );
	return ERR;
    }
		/* read headblock,datablocks from file and send to IC35 */
    fwlen = 0;
    for ( i = 0; i <= NDATABLKS; ++i ) {
	char		btext[24+1];
	size_t		blen;
	if ( i == 0 ) {
	    fprintf( stderr, "write database <- %s head  ", fname );
	    strcpy( btext, "headblock" );
	    blen = HEADBLKSZ;
	} else {
	    sprintf( btext, "datablock-%d", i );
	    blen = DATABLKSZ;
	}
	if ( fread( buff, sizeof(buff[0]), blen, fp ) != blen ) {
	    fprintf( stderr, "\n" );
	    error( "writedatabase %s %s: read error %s (%d)",
			    btext, fname, strerror(errno), errno );
	    return ERR;
	}
	if ( (wlen = Msendblk( buff, blen )) != blen ) {
	    fprintf( stderr, "\n" );
	    error( "writedatabase %s %s", btext, _dbetext( wlen ) );
	    return ERR;
	}
	fwlen += wlen;
	fprintf( stderr, "\rwrite database <- %s %ldk  ",
				  fname, (fwlen+511)/1024 );
    }
    fprintf( stderr, "\n" );
    usleep( 3250000 );		/* wait 3.25 sec for IC35 doing restore */

    LPRINTF(( L_INFO, "writedatabase %s OK", fname ));
    return OK;
}

/* write IC35 database: init,free resources
*  ----------------------------------------
*/
int
writedatabase( char * fname )
{
    uchar *	buff;
    FILE *	fp;
    int 	rval;

    if ( (buff = malloc( DATABLKSZ )) != NULL ) {
	if ( fname && *fname
	  && (fp = fopen( fname, "r" )) != NULL ) {
	    rval = _writedatabase( fname, fp, buff );
	    fclose( fp );
	} else {
	    error( "writedatabase cannot open %s", fname ? fname : "(NULL)" );
	    rval = ERR;
	}
	free( buff );
    } else {
	error( "writedatabase failed: no memory (%d)", DATABLKSZ );
	rval = ERR;
    }
    return rval;
}
