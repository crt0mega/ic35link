/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*/
	static char rcsid[] =
	"$Id: dataio.c,v 1.33 2001/03/02 02:09:59 tsch Rel $";  	/*
*									*
* IC35 synchronize data import/export					*
*									*
*************************************************************************
*									*
*	??? is "fixme" mark: sections of code needing fixes		*
*									*
*	backup_and_openwr	backup and open output file for write	*
*	    revised date+time on IC35					*
*	clr_newic35dt		clear new IC35 revised to get it fresh	*
*	get_newic35dt		new revised date+time to IC35 string	*
*	set_oldic35dt		IC35 string to old revised date+time	*
*									*
*	    PIM format
*	pimfmt2bin		local: PIM format text to binary
*	set_pim_format		set PIM format
*	pim_format		report current PIM format
*	pimop[]			functions table of format-specific ops
*	    PIM file access
*	pim_open		open PIMfile(s) and read if present
*	pim_openinp		open PIMfile(s) for input and read
*	pim_openout		note PIMfile(s) for output
*	pim_close		close and flush to PIMfile(s)
*	    PIM record operations
*	pim_rewind		rewind PIM record list
*	pim_getrec		get next PIM record for fileid
*	pim_getrec_byID		get PIM record by rec-ID (and fileid)
*	pim_updic35rec		update IC35 record with PIM record
*	pim_cmpic35rec		compare IC35 record with PIM record
*	pim_putic35rec		put IC35 record to (new) PIM record
*	pim_putrec		output (new) PIM record
*	pim_delrec		delete PIM record
*	    PIM record IC35-recID and status
*	pim_recid		report IC35 record-ID from PIM record
*	pim_set_recid		set IC35 record-ID to PIM record
*	pim_recstat		report PIM record changeflag
*	pim_set_recstat		set PIM record changeflag
*									*
************************************************************************/

#include <stdio.h>	/* fprintf(), ..	*/
#include <string.h>	/* strcpy(), ..		*/
#include <ctype.h>	/* isprint(), ..	*/
#include <unistd.h>	/* access()		*/
#include <sys/types.h>	/* size_t, ..		*/
#include <time.h>	/* struct tm, time() ..	*/

#include "vcc.h"	/* VObject, ..		*/
#include "util.h"	/* ERR, uchar, ..	*/
#include "ic35frec.h"	/* IC35 record fields.. */
#include "dataio.h"
NOTUSED(rcsid);


/* backup and open output file for write
*  -------------------------------------
*/
FILE *
backup_and_openwr( char * fname )
{
    char *	bkpfname;
    FILE *	fp;

    if ( !( fname && *fname ) )
	return NULL;
    if ( strcmp( fname, "-" ) == 0 )
	return stdout;
    if ( (bkpfname = malloc( strlen( fname ) + 2 )) != NULL ) {
	strcat( strcpy( bkpfname, fname ), "~" );
	remove( bkpfname );
	rename( fname, bkpfname );
	free( bkpfname );
    }
    if ( (fp = fopen( fname, "w" )) == NULL )
	error( "cannot open outfile: %s", fname );
    return fp;
}


/* ============================================ */
/*	revised date+time on IC35		*/
/* ============================================ */

/*
*	IC35 does not maintain last modified date+time per record
*	instead there is a command to write date+time to IC35,
*	which is used for synchronization.
*	this timestamp is also used for "last modified" VObject property
*/

/* revised date+time for vCard,vCal records
*  ----------------------------------------
*/
static time_t	_new_ic35dtime;	/* to vCard,vCal records and IC35 */

void
clr_newic35dt( void )
{
    _new_ic35dtime = 0;
}
time_t
newic35dt( void )
{
    if ( _new_ic35dtime == 0 )
	_new_ic35dtime = time( NULL );
    return _new_ic35dtime;
}

/* new revised date+time to string for IC35
*  ----------------------------------------
*/
void
get_newic35dt( char * dtbuf )
{
    time_t	ic35dt;
    struct tm *	ptm;

    ic35dt = newic35dt();
    ptm = localtime( &ic35dt );
    strftime( dtbuf, 2+2+4+2+2+2+1, "%m%d%Y%H%M%S", ptm );
}

/* string from IC35 to old revised date+time
*  -----------------------------------------
*	the ugly format [mmddyyyyhhmmss] (instead of pretty [yyyymmddhhmmss])
*	is used for reasons of compatibility with IC35sync/Windows.
*	if sysinfo string from IC35 lacks plausible year,month,day, assume
*	sysinfo was never written to IC35 and use old reference date+time
*	far in the past.
*/
static time_t	_old_ic35dtime;	/* from IC35, reference for vCard */

void
set_oldic35dt( char * dtime )
{
    struct tm	ic35tm;

    if ( dtime == NULL )
	return;
    memset( &ic35tm, 0, sizeof(ic35tm) );
    sscanf( dtime, "%2d%2d%4d%2d%2d%2d",
			&ic35tm.tm_mon, &ic35tm.tm_mday, &ic35tm.tm_year,
			&ic35tm.tm_hour, &ic35tm.tm_min, &ic35tm.tm_sec );
    if ( 1970 <= ic35tm.tm_year
      && 1 <= ic35tm.tm_mon  && ic35tm.tm_mon  <= 12
      && 1 <= ic35tm.tm_mday && ic35tm.tm_mday <= 31 ) {
	ic35tm.tm_year -= 1900;
	ic35tm.tm_mon  -= 1;
	ic35tm.tm_isdst = -1;
	_old_ic35dtime = mktime( &ic35tm );
    } else
	_old_ic35dtime = 1;
}
time_t
oldic35dt( void )
{
    return _old_ic35dtime;
}


/* ============================================ */
/*	generic PIM access			*/
/* ============================================ */

/* set, get format of input/output file(s)
*  ---------------------------------------
*/
static int	pimfmt;
static int	inpfmt;

static int
pimfmt2bin( char * format )
{
    if ( format ) {
	if (      strcmp( format, "txt" ) == 0 )  return PIM_TXT;
	else if ( strcmp( format, "bin" ) == 0 )  return PIM_BIN;
	else if ( strcmp( format, "vca" ) == 0 )  return PIM_VCA;
    }
    return 0;
}
int
set_pim_format( char * format )
{
    int 	newfmt;

    if ( (newfmt = pimfmt2bin( format )) <= 0 )
	return ERR;				/* unknown format */
    pimfmt = inpfmt = newfmt;
    return OK;
}
int
pim_format( void )
{
    return pimfmt;
}

/*
*	pim_open()
*	pim_close()
*	pim_rewind()
*
*	pim_getrec( int fileid )		-> void* / NULL
*	pim_getrec_byID( ulong recid )		-> void* / NULL
*	pim_cmpic35rec( IC35REC*, void* )	-> 0:same 1:differ
*	pim_putic35rec( IC35REC* )		IC35REC to PIMrec
*	pim_updic35rec( IC35REC*, void* )	PIMrec to IC35REC
*	pim_delrec( void* )
*
*	pim_recid( void* )
*	pim_set_recid( void*, ulong )
*	pim_recstat( void* )
*	pim_set_recstat( void*, int )
*/

/* dummy null operations
*  ---------------------
*/
static struct pim_oper	nul_oper = {
			    NULL,		/* NO open	  */
			    NULL,		/* NO close	  */
			    NULL,		/* NO rewind	  */
			    NULL,		/* NO getrec	  */
			    NULL,		/* NO getrec_byID */
			    NULL,		/* NO cmpic35rec  */
			    NULL,		/* NO updic35rec  */
			    NULL,		/* NO putic35rec  */
			    NULL,		/* NO delrec	  */
			    NULL,		/* NO recid	  */
			    NULL,		/* NO set_recid   */
			    NULL,		/* NO recstat	  */
			    NULL,		/* NO set_recstat */
			};

/* table of supported PIM-formats
*  ------------------------------
*/
extern struct pim_oper	txt_oper;	/* text output IC35 record	*/
extern struct pim_oper	bin_oper;	/* binary IC35 record format	*/
extern struct pim_oper	vca_oper;	/* vCard,vCalendar format	*/

static struct pim_oper *pimop[] = {
			    &nul_oper,
			    &txt_oper,
			    &bin_oper,
			    &vca_oper
			};

/* open, close, rewind output file(s)
*  ----------------------------------
*/
int
pim_open( char * addrfname, char * vcalfname, char * memofname )
{
    LPRINTF(( L_INFO, "pim_open(%s,%s,%s) pimfmt=%d (%s)",
			addrfname ? addrfname : "NULL",
			vcalfname ? vcalfname : "NULL",
			memofname ? memofname : "NULL",
			pimfmt, pimfmt == PIM_TXT ? "txt" :
				  pimfmt == PIM_BIN ? "bin" :
				    pimfmt == PIM_VCA ? "vca" : "unknown" ));
/*??? check file(s) creatable in dirname(a_xxxxfname) ???*/
    if ( pimop[pimfmt]->open )
	return (*pimop[pimfmt]->open)( "r+", addrfname, vcalfname, memofname );
    return ERR;
}
int
pim_openinp( char * format, char * fname )
{
    int 	ifmt;

    if ( (ifmt = pimfmt2bin( format )) <= 0
      || pimop[ifmt]->open == NULL )
	return ERR;
    inpfmt = ifmt;
    return (*pimop[inpfmt]->open)( "r", fname, NULL, NULL );
}
int
pim_openout( char * format, char * fname )
{
    int 	ofmt;

    if ( (ofmt = pimfmt2bin( format )) <= 0
      || pimop[ofmt]->open == NULL
      || !( fname && *fname ) )
	return ERR;
    pimfmt = ofmt;
/*??? check file creatable in dirname(fname) ???*/
    remove( fname );
    return (*pimop[pimfmt]->open)( "w", fname, NULL, NULL );
}
void
pim_close( void )
{
    LPRINTF(( L_INFO, "pim_close: pimfmt=%d (%s) inpfmt=%d (%s)",
			pimfmt, pimfmt == PIM_TXT ? "txt" :
				  pimfmt == PIM_BIN ? "bin" :
				    pimfmt == PIM_VCA ? "vca" : "unknown",
			inpfmt, inpfmt == PIM_TXT ? "txt" :
				  inpfmt == PIM_BIN ? "bin" :
				    inpfmt == PIM_VCA ? "vca" : "unknown" ));
    if ( pimop[pimfmt]->close ) 	/* write output file and ..	*/
	(*pimop[pimfmt]->close)();	/*  release output record list	*/
    if ( inpfmt != pimfmt		/* input format is different ..	*/
      && *pimop[inpfmt]->close )	/*  release input record list	*/
	(*pimop[inpfmt]->close)();
}
void
pim_rewind( void )
{
    if ( pimop[inpfmt]->rewind )
	(*pimop[inpfmt]->rewind)();
}

/* get next PIM-record for fileid
*  ------------------------------
*/
void *
pim_getrec( int fileid )
{
    if ( pimop[inpfmt]->getrec )
	return (*pimop[inpfmt]->getrec)( fileid );
    return NULL;
}
/* get PIM-record by rec-ID for fileid
*  -----------------------------------
*/
void *
pim_getrec_byID( ulong recid )
{
    if ( pimop[inpfmt]->getrec_byID )
	return (*pimop[inpfmt]->getrec_byID)( recid );
    return NULL;
}

/* update IC35 record with PIM record
*  ----------------------------------
*/
IC35REC *
pim_updic35rec( IC35REC * ic35rec, void * pimrec )
{
    if ( pimop[inpfmt]->updic35rec )
	return (*pimop[inpfmt]->updic35rec)( ic35rec, pimrec );
    return NULL;
}

/* compare IC35 record with vCard,vCal record
*  ------------------------------------------
*/
int
pim_cmpic35rec( IC35REC * ic35rec, void * pimrec )
{
    if ( pimop[pimfmt]->cmpic35rec )
	return (*pimop[pimfmt]->cmpic35rec)( ic35rec, pimrec );
    return -1;
}

/* put IC35-record to (new) PIM-record
*  -----------------------------------
*/
void *
pim_putic35rec( IC35REC * ic35rec )
{
    if ( pimop[pimfmt]->putic35rec )
	return (*pimop[pimfmt]->putic35rec)( ic35rec );
    return NULL;
}
void
pim_putrec( void * pimrec )
{
    IC35REC *	ic35rec;

    if ( pimop[inpfmt]->updic35rec != NULL
      && pimop[pimfmt]->putic35rec != NULL
      && (ic35rec = new_ic35rec()) != NULL ) {
	(*pimop[inpfmt]->updic35rec)( ic35rec, pimrec );
	(*pimop[pimfmt]->putic35rec)( ic35rec );
	del_ic35rec( ic35rec );
    }
}

/* delete PIM-record
*  -----------------
*/
void
pim_delrec( void * pimrec )
{
    if ( pimop[pimfmt]->delrec )
	(*pimop[pimfmt]->delrec)( pimrec );
}

/* get,set IC35-record-ID
*  ----------------------
*/
ulong
pim_recid( void * pimrec )
{
    if ( pimop[pimfmt]->recid )
	return (*pimop[pimfmt]->recid)( pimrec );
    return 0;
}
void
pim_set_recid( void * pimrec, ulong recid )
{
    if ( pimop[pimfmt]->set_recid )
	(*pimop[pimfmt]->set_recid)( pimrec, recid );
}

/* get,set record-changeflag
*  -------------------------
*/
int
pim_recstat( void * pimrec )
{
    if ( pimop[pimfmt]->recstat )
	return (*pimop[pimfmt]->recstat)( pimrec );
    return PIM_DIRTY;
}
void
pim_set_recstat( void * pimrec, int stat )
{
    if ( pimop[pimfmt]->set_recstat )
	(*pimop[pimfmt]->set_recstat)( pimrec, stat );
}
