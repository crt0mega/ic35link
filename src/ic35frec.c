/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*/
	static char rcsid[] =
	"$Id: ic35frec.c,v 1.8 2001/03/02 02:09:59 tsch Rel $";  		/*
*									*
* IC35 record access							*
*									*
*************************************************************************
*									*
*	??? is "fixme" mark: sections of code needing fixes		*
*									*
************************************************************************/

#include <stdlib.h>	/* calloc(), ..		*/
#include <string.h>	/* strcmp(), ..		*/

#include "util.h"	/* ERR,OK, uchar, ..	*/
#include "ic35frec.h"
NOTUSED(rcsid);


struct ic35file {	/* IC35 file description		*/
    int 	id;		/*  id on IC35			*/
    int 	nfld;		/*  number of fields in record	*/
    char *	name;		/*  filename			*/
};
struct ic35rec {	/* internal IC35 record for field access	*/
    ulong	id;		/* record-id on IC35: recid[3],fileid[1]*/
    uchar	chg;		/* change-flag on IC35			*/
    size_t	dlen;		/* length of IC35 record data buffer	*/
    uchar *	data;		/* record data buffer: flens, fdata	*/
    int		nflds;		/* number of fields in record		*/
    struct {
	size_t	  len;		/*  length of field			*/
	uchar *   ptr;		/*  pointer to field data		*/
    }		flds[MAXFLDS];	/* vector of record fields		*/
    size_t	fblen;		/* length of field data buffer		*/
    uchar *	fbuff;		/* field data buffer			*/
};


/* lookup IC35 fileid
*  ------------------
*/
static struct ic35file 	ic35files[] = {
    { FILEADDR,  21, "Addresses"  },
    { FILEMEMO,   4, "Memo"	  },
    { FILESCHED, 10, "Schedule"   },
    { FILETODO,   8, "To Do List" },
    {	0,	  0,	NULL	  }
};

static struct ic35file *
_ic35fdesc( int fileid )
{
    struct ic35file *	pfile;

    for ( pfile = ic35files; pfile->name != NULL; ++pfile )
	if ( pfile->id == fileid )
	    return pfile;
    return NULL;
}

/* get number of record fields acc.to file-id
*  ------------------------------------------
*/
int
ic35fnflds( int fileid )
{
    struct ic35file *	pfile;

    pfile = _ic35fdesc( fileid );
    if ( pfile == NULL )		/* unknown file id	*/
	return 0;
    return pfile->nfld;
}
static int
_nrecflds( int fldid )
{
    int 	nflds;

    if ( (nflds = ic35fnflds( FldFile(fldid) )) == 0	/* unkwown file id */
      || FldIdx(fldid) >= nflds				/* bad field index */
      || FldIdx(fldid) >= alenof(((IC35REC*)0)->flds) )
	return 0;
    return nflds;
}

/* get filename acc.to file-id
*  ---------------------------
*/
char *
ic35fname( int fileid )
{
    struct ic35file *	pfile;

    pfile = _ic35fdesc( fileid );
    if ( pfile == NULL )		/* unknown file id	*/
	return NULL;
    return pfile->name;
}


/* constructur: create new IC35 record
*  -----------------------------------
*/
IC35REC *
new_ic35rec( void )
{
    return calloc( 1, sizeof(IC35REC) );
}

/* destructor: delete IC35 record
*  ------------------------------
*/
static void
_del_ic35recbuffs( IC35REC * rec )
{
    int 	fi;

    for ( fi = 0; fi < alenof(rec->flds); ++fi ) {
	if ( !( rec->fbuff <= rec->flds[fi].ptr
	  &&		      rec->flds[fi].ptr < rec->fbuff+rec->fblen ) )
	    free( rec->flds[fi].ptr );
	rec->flds[fi].ptr = NULL;
	rec->flds[fi].len = 0;
    }
    rec->nflds = 0;
    if ( rec->fbuff != NULL ) {
	free( rec->fbuff );
	rec->fbuff = NULL;
	rec->fblen = 0;
    }
    if ( rec->data != NULL ) {
	free( rec->data );
	rec->data = NULL;
	rec->dlen = 0;
    }
}
void
del_ic35rec( IC35REC * rec )
{
    _del_ic35recbuffs( rec );
    free( rec );
}

/* set,get special field in IC35 record
*  ------------------------------------
*/
void
set_ic35recid( IC35REC * rec, ulong recid )
{
    int 	nflds;

    rec->id = recid;
    if ( (nflds = ic35fnflds( FileId(recid) )) != 0 )
	rec->nflds = nflds;
}
ulong
ic35recid( IC35REC * rec )
{
    return rec->id;
}
void
set_ic35recchg( IC35REC * rec, uchar chgflag )
{
    rec->chg = chgflag;
}
uchar
ic35recchg( IC35REC * rec )
{
    return rec->chg;
}

/* set,get IC35 record raw data
*  ----------------------------
*/
void
set_ic35recdata( IC35REC * rec, uchar * data, size_t dlen )
{
    uchar *		fptr;
    uchar *		fbptr;
    int 		fi;
    size_t		nflds, flensum;

    _del_ic35recbuffs( rec );
    if ( data == NULL || dlen == 0 )
	return;
    if ( (rec->data = malloc( dlen )) == NULL )
	return;
    memcpy( rec->data, data, rec->dlen = dlen );
    memset( rec->flds, 0, sizeof(rec->flds) );
    for ( nflds = flensum = 0; nflds < alenof(rec->flds); ++nflds ) {
	flensum += rec->data[nflds] + 1;
	if ( flensum > dlen )
	    break;
    }
    if ( (rec->fbuff = malloc( dlen )) == NULL )
	return;
    rec->fblen = dlen;
    rec->nflds = nflds;
    fbptr = rec->fbuff;
    fptr = rec->data + nflds;
    for ( fi = 0; fi < nflds; ++fi ) {
	rec->flds[fi].len = rec->data[fi];
	rec->flds[fi].ptr = fbptr;
	memcpy( rec->flds[fi].ptr, fptr, rec->flds[fi].len );
	fbptr += rec->flds[fi].len;
	*fbptr++ = '\0';		/* append end-of-string	*/
	fptr += rec->data[fi];
    }
}
void
get_ic35recdata( IC35REC * rec, uchar ** pdata, size_t * pdlen )
{
    uchar *		fptr;
    int 		fi;
    size_t		dlen;

    if ( rec->data == NULL ) {		/* rebuild record data	*/
	for ( fi = 0, dlen = 0; fi < rec->nflds; ++fi )
	    dlen += rec->flds[fi].len + 1;
	if ( (rec->data = malloc( dlen )) != NULL ) {
	    rec->dlen = dlen;
	    fptr = rec->data + rec->nflds;
	    for ( fi = 0; fi < rec->nflds; ++fi ) {
		if ( rec->flds[fi].ptr != NULL )
		    memcpy( fptr, rec->flds[fi].ptr, rec->flds[fi].len );
		else
		    rec->flds[fi].len = 0;
		fptr += (rec->data[fi] = rec->flds[fi].len);
	    }
	}
    }
    if ( pdata )
	*pdata = rec->data;
    if ( pdlen )
	*pdlen = rec->dlen;
}

/* set,get string field from IC35 record
*  -------------------------------------
*/
void
set_ic35recfld( IC35REC * rec, int fldid, char * data )
{
    int 	fi, nflds;
    size_t	flen;
    char *	fbuff;

    if ( (nflds = _nrecflds( fldid )) <= 0 )
	return;						/* bad field id	*/
    if ( rec->nflds == 0 )
	rec->nflds = nflds;
    if ( (fi = FldIdx(fldid)) >= rec->nflds )
	return;
    if ( data == NULL )
	data = "";
    switch ( fldid ) {
    case A_CategoryID:
    case S_AlarmBefore:
    case S_Alarm_Repeat:
    case S_RepCount:
    case T_Completed:
    case T_Priority:
    case T_CategoryID:
    case M_CategoryID:			/* 1-byte binary fields		*/
	flen = 1;
	break;
    default:
	flen = strlen( data );
    }
    if ( flen > 255 ) flen = 255;	/* truncate to max.field length */
    if ( flen > rec->flds[fi].len ) {	/* new field bigger old content */
	if ( (fbuff = malloc( flen + 1 )) == NULL )
	    return;					/* no memory	*/
	if ( !( rec->fbuff <= rec->flds[fi].ptr
	      &&	      rec->flds[fi].ptr < rec->fbuff+rec->fblen ) )
	    free( rec->flds[fi].ptr );	/* old field outside buffer	*/
	rec->flds[fi].ptr = fbuff;
    }
    if ( flen ) {
	memcpy( rec->flds[fi].ptr, data, flen );
	rec->flds[fi].ptr[flen] = '\0';
    }
    rec->flds[fi].len = flen;
    if ( rec->data != NULL ) {
	free( rec->data );
	rec->data = NULL;		/* record data must be rebuilt	*/
	rec->dlen = 0;
    }
}
void
get_ic35recfld( IC35REC * rec, int fldid, uchar ** pfld, size_t * plen )
{
    if ( _nrecflds( fldid ) <= 0			/* bad field id	*/
      || rec->flds[FldIdx(fldid)].ptr == NULL ) {	/*  empty field	*/
	if ( pfld ) *pfld = "";
	if ( plen ) *plen = 0;
    } else {
	if ( pfld ) *pfld = rec->flds[FldIdx(fldid)].ptr;
	if ( plen ) *plen = rec->flds[FldIdx(fldid)].len;
    }
}
char *
ic35recfld( IC35REC * rec, int fldid )
{
    uchar *	fld;

    get_ic35recfld( rec, fldid, &fld, NULL );
    return fld;
}

/* compare 2 IC35 records
*  ----------------------
*	records are compared field by field, because some fields need
*	special handling, i.e. difference ignored.
*   returns
*	0	both IC35 records are (regarded) same
*	1	IC35 records differ
*	-1	IC35 records differ strangely:
*		either one is NULL or fileids mismatch
*/
int
cmp_ic35rec( IC35REC * rec1, IC35REC * rec2 )
{
    int 	fileid;
    int 	fi, nflds, fldid;
    uchar *	ic35fld;
    uchar *	binfld;
    char	ic35tfld[6+1];

    if ( rec1 == NULL && rec2 == NULL )
	return 0;
    if ( rec1 == NULL || rec2 == NULL )
	return -1;
    if ( (fileid = FileId( ic35recid( rec1 ) ))
		!= FileId( ic35recid( rec2  ) ) )
	return -1;

    nflds = ic35fnflds( fileid );
    for ( fi = 0; fi < nflds; ++fi ) {
	fldid = FILEfld(fileid,fi);
	ic35fld = ic35recfld( rec1, fldid );
	binfld  = ic35recfld( rec2,  fldid );
	if ( strcmp( binfld, ic35fld ) == 0 )
	    continue;
	switch ( fldid ) {
	case A_CategoryID:
	case M_CategoryID:
	case T_CategoryID:
	    continue;			/* ignore category-ID difference */
	case S_StartTime:
	case S_EndTime:
	    ic35fld = strncat( strcpy( ic35tfld, "" ),
				ic35fld, sizeof(ic35tfld)-1 );
	    if ( ( fldid == S_StartTime && ic35fld[4] == ':' )
	      || ( fldid == S_EndTime   && ic35fld[4] == '<' ) )
		ic35fld[4] = ic35fld[5] = '0';
	    if ( strcmp( ic35fld, binfld ) == 0 )
		continue;		/* ignore strange IC35 Start/EndTime */
	    break;
	case S_RepEndDate:
	case S_RepCount:
	    if ( (*ic35recfld( rec2, S_Alarm_Repeat ) & RepeatMASK) == 0 )
		continue;		/* ignore difference if not repeat */
	    break;
	case S_Alarm_Repeat:
	    if ( ((*binfld ^ *ic35fld) & ~(AlarmNoLED|AlarmNoBeep)) == 0
	      && *ic35recfld( rec2, S_AlarmBefore ) == AlarmBefNone )
		continue;		/* ignore LED,Beep diff if no alarm */
	    break;
	}
	LPRINTF(( L_DEBUG, "cmp_ic35rec: rid=%08lX fi=%d diff",
			    ic35recid(rec1), fi ));
	LDUMP(( L_DEBUG, binfld, strlen(binfld),
			  "cmp_ic35rec: rec2  len=%d", strlen(binfld) ));
	LDUMP(( L_DEBUG, ic35fld, strlen(ic35fld),
			  "cmp_ic35rec: rec1 len=%d", strlen(ic35fld) ));
	return 1;
    }
    return 0;
}
