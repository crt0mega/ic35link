/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*/
	static char rcsid[] =
	"$Id: databin.c,v 1.35 2001/03/02 02:09:59 tsch Rel $";  	/*
*									*
* IC35 synchronize data import/export: binary IC35 record format	*
*									*
*************************************************************************
*									*
*	??? is "fixme" mark: sections of code needing fixes		*
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


#pragma pack(1)
struct binrec {		/* IC35 binary record data	*/
    size_t	len;	/*  length of this record	*/
    ulong	rid;	/*  record id on IC35		*/
    uchar	chg;	/*  changeflag on IC35		*/
};			/*  flengths,fdata (dyn.length)	*/
#pragma pack()
struct binelmt {	/* IC35 binrec list element	*/
    struct binelmt *	next;	/* next element	in list	*/
    IC35REC *		rec;	/* IC35 record		*/
};

static struct binelmt *	binlist;
static struct binelmt * binget;

/* internal: write IC35 binary record to file
*  ------------------------------------------
*/
static int
_bin_writerec( FILE * outfp, IC35REC * rec )
{
    uchar *		recdata;
    size_t		reclen;
    struct binrec	brec;

    get_ic35recdata( rec, &recdata, &reclen );
    brec.len = sizeof(brec) + reclen;
    brec.rid = ic35recid( rec );
    brec.chg = ic35recchg( rec );
    if ( fwrite( &brec, sizeof(brec), 1, outfp ) != 1
      || fwrite( recdata, reclen, 1, outfp ) != 1 )
	return ERR;
    return OK;
}
/* internal: read IC35 binary record from file
*  -------------------------------------------
*/
static int
_bin_readrec( FILE * infp, IC35REC * rec )
{
    struct binrec	brec;
    uchar *		rbuff;
    size_t		rlen, i;

    if ( fread( &brec, sizeof(brec), 1, infp ) != 1 )
	return -1;
    set_ic35recid( rec, brec.rid );
    set_ic35recchg( rec, brec.chg );
    rlen = brec.len - sizeof(brec);
    if ( rlen > 0
      && (rbuff = malloc( rlen )) != NULL ) {
	if ( fread( rbuff, rlen, 1, infp ) == 1 )
	    set_ic35recdata( rec, rbuff, rlen );
	else
	    rlen = ERR;
	free( rbuff );
    } else {
	for ( i = 0; i < rlen; ++i )
	    (void)fgetc( infp );
    }
    return rlen;
}

/* open IC35 binary record file
*  ----------------------------
*	read input file into internal record list
*/
static char *	iomode;
static char *	addr_fname;

static int
bin_open( char * mode, char * addrfname, char * vcalfname, char * memofname )
{
    FILE *		infp;
    IC35REC *		ic35rec;
    struct binelmt *	newelmt;
    struct binelmt *	pelmt;

    LPRINTF(( L_INFO, "bin_open(%s,%s,%s)",
			addrfname ? addrfname : "NULL",
			vcalfname ? vcalfname : "NULL",
			memofname ? memofname : "NULL" ));
    iomode = mode;			/* note input/output for close	*/
    addr_fname = addrfname;		/* note filename for close	*/
    binlist = binget = NULL;		/* reset record list		*/
    if ( !( addrfname && *addrfname )
      || iomode[0] != 'r' )		/* open for output only		*/
	return OK;
    if ( strcmp( addrfname, "-" ) == 0 )
	infp = stdin;
    else if ( (infp = fopen( addrfname, "r" )) == NULL )
	return access( addrfname, F_OK ) == 0 ? ERR : OK;
    LPRINTF(( L_INFO, "bin_open: read %s ..", addrfname ));
    pelmt = NULL;
    for ( ; ; ) {
	if ( (ic35rec = new_ic35rec()) == NULL
	  || _bin_readrec( infp, ic35rec ) < 0 )
	    break;
	if ( (newelmt = malloc( sizeof(*newelmt) )) != NULL ) {
	    newelmt->next = NULL;
	    newelmt->rec = ic35rec;
	    if ( pelmt == NULL )
		binlist = pelmt = newelmt;
	    else
		pelmt = pelmt->next = newelmt;
	}
    }
    if ( infp != stdin )
	fclose( infp );
    del_ic35rec( ic35rec );
    LPRINTF(( L_INFO, "bin_open: read %s done", addrfname ));
    return OK;
}
/* close IC35 binary record file
*  -----------------------------
*	write internal record list to output file
*	if closing input file only release record list
*/
static void
bin_close( void )
{
    FILE *		outfp = NULL;
    struct binelmt *	delelmt;

    if ( addr_fname && *addr_fname
      && (iomode[0] == 'w' || iomode[1] == '+')
      && (outfp = backup_and_openwr( addr_fname )) == NULL )
	return;
    LPRINTF(( L_INFO, "bin_close: %s",
			outfp && binlist ? "write .." : "no output" ));
    LPRINTF(( L_INFO, "bin_close: write %s ..", addr_fname ));
    while ( binlist ) {
	if ( outfp )
	    _bin_writerec( outfp, binlist->rec );
	delelmt = binlist;
	binlist = binlist->next;
	del_ic35rec( delelmt->rec );
	free( delelmt );
    }
    binget = NULL;		/* sanity for bin_getrec()     */
    if ( outfp && outfp != stdout )
	fclose( outfp );
    LPRINTF(( L_INFO, "bin_close: done" ));
}

/* rewind list of binary records
*  -----------------------------
*/
static void
bin_rewind( void )
{
    binget = NULL;
}

/* get binary record for fileid
*  ----------------------------
*/
static IC35REC *
bin_getrec( int fileid )
{
    if ( binget == NULL )
	binget = binlist;
    else
	binget = binget->next;
    while ( binget ) {
	if ( fileid == FILE_ANY		/* next record with any fileid	*/
	  || FileId( ic35recid( binget->rec ) ) == fileid )
	    return binget->rec;
	binget = binget->next;
    }
    return NULL;
}
/* get binary record by record-ID
*  ------------------------------
*/
static IC35REC *
bin_getrec_byID( ulong recid )
{
    struct binelmt *	pelmt;

    if ( RecId( recid ) == 0 )
	return NULL;
    for ( pelmt = binlist; pelmt; pelmt = pelmt->next )
	if ( ic35recid( pelmt->rec ) == recid )
	    return pelmt->rec;
    return NULL;
}

/* compare IC35 record with binary record
*  --------------------------------------
*/
static int
bin_cmpic35rec( IC35REC * ic35rec, IC35REC * binrec )
{
    return cmp_ic35rec( ic35rec, binrec );
}

/* update IC35 record with binary record
*  -------------------------------------
*/
static IC35REC *
bin_updic35rec( IC35REC * ic35rec, IC35REC * rec )
{
    uchar *	data;
    size_t	dlen;

    if ( ic35rec == NULL
      && (ic35rec = new_ic35rec()) == NULL )
	return NULL;
    set_ic35recid( ic35rec, ic35recid( rec ) );
    get_ic35recdata( rec, &data, &dlen );
    set_ic35recdata( ic35rec, data, dlen );
    return ic35rec;
}

/* put IC35 record to (new) binary record
*  --------------------------------------
*/
static IC35REC *
bin_putic35rec( IC35REC * ic35rec )
{
    struct binelmt *	pelmt;
    struct binelmt *	newelmt;
    struct binelmt *	lastinfile;
    struct binelmt *	last;
    IC35REC *		rec;
    uchar *		data;
    size_t		dlen;

    if ( (rec = bin_getrec_byID( ic35recid( ic35rec ) )) == NULL ) {
	if ( (newelmt = malloc( sizeof(*newelmt) )) == NULL
	  || (rec = newelmt->rec = new_ic35rec()) == NULL ) {
	    free( newelmt );
	    return NULL;
	}
	last = lastinfile = NULL;
	for ( pelmt = binlist; pelmt; pelmt = pelmt->next ) {
	    if ( FileId( ic35recid( pelmt->rec ) )
	      == FileId( ic35recid( ic35rec ) ) )
		lastinfile = pelmt;
	    last = pelmt;
	}
	if ( lastinfile ) last = lastinfile;
	if ( last == NULL ) {
	    binlist = newelmt;
	    newelmt->next = NULL;
	} else {
	    newelmt->next = last->next;
	    last->next = newelmt;
	}
    }
    set_ic35recid( rec, ic35recid( ic35rec ) );
    get_ic35recdata( ic35rec, &data, &dlen );
    set_ic35recdata( rec, data, dlen );
    return rec;
}

/* delete binary record
*  --------------------
*/
static void
bin_delrec( IC35REC * delrec )
{
    struct binelmt **	pnext;
    struct binelmt *	pelmt;

    for ( pnext = &binlist, pelmt = *pnext; pelmt != NULL;
	  pnext = &pelmt->next, pelmt = *pnext )
	if ( pelmt->rec == delrec ) {
	    *pnext = pelmt->next;
	    del_ic35rec( pelmt->rec );
	    free( pelmt );
	    break;
	}
}

/* get,set binary record-ID
*  ------------------------
*/
static ulong
bin_recid( IC35REC * rec )
{
    return ic35recid( rec );
}
static void
bin_set_recid( IC35REC * rec, ulong recid )
{
    set_ic35recid( rec, recid );
}

/* get,set binary record status
*  ----------------------------
*/
static int
bin_recstat( IC35REC * rec )
{
    switch ( ic35recchg( rec ) ) {
    case IC35_CLEAN:	return PIM_CLEAN;
    case IC35_NEW:
    case IC35_MOD:	return PIM_DIRTY;
    case IC35_DEL:	return PIM_DEL;
    }
    return PIM_DIRTY;
}
static void
bin_set_recstat( IC35REC * rec, int stat )
{
    switch ( stat ) {
    case PIM_CLEAN:  set_ic35recchg( rec, IC35_CLEAN ); break;
    case PIM_DIRTY:  set_ic35recchg( rec, IC35_NEW );	break;
    case PIM_DEL:    set_ic35recchg( rec, IC35_DEL );	break;
    }
}


/* binary format operations
*  ------------------------
*/
struct pim_oper	bin_oper = {
				bin_open,
				bin_close,
				bin_rewind,
		 (void*(*)(int))bin_getrec,
	       (void*(*)(ulong))bin_getrec_byID,
	(int(*)(IC35REC*,void*))bin_cmpic35rec,
   (IC35REC*(*)(IC35REC*,void*))bin_updic35rec,
	    (void*(*)(IC35REC*))bin_putic35rec,
		(void(*)(void*))bin_delrec,
	       (ulong(*)(void*))bin_recid,
	  (void(*)(void*,ulong))bin_set_recid,
		 (int(*)(void*))bin_recstat,
	    (void(*)(void*,int))bin_set_recstat,
			};

