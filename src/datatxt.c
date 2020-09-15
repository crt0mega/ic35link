/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*/
	static char rcsid[] =
	"$Id: datatxt.c,v 1.32 2000/12/26 01:36:32 tsch Rel $";  	/*
*									*
* IC35 synchronize data import/export: text format output		*
*									*
*************************************************************************
*									*
*	??? is "fixme" mark: sections of code needing fixes		*
*									*
************************************************************************/

#include <stdio.h>	/* fprintf(), ..	*/
#include <ctype.h>	/* isprint(), ..	*/
#include <sys/types.h>	/* size_t, ..		*/

#include "util.h"	/* ERR, uchar, ..	*/
#include "ic35frec.h"	/* IC35 record fields.. */
#include "dataio.h"	/* struct pim_oper	*/
NOTUSED(rcsid);


/* open, close text file
*  ---------------------
*/
static FILE *	outfp;

static int
txt_open( char * mode, char * addrfname, char * vcalfname, char * memofname )
{
    if ( !(mode[0] == 'w' || mode[1] == '+')
      || (outfp = backup_and_openwr( addrfname )) == NULL )
	return ERR;
    return OK;
}
static void
txt_close( void )
{
    if ( outfp && outfp != stdout )
	fclose( outfp );
    outfp = NULL;
}

/* output IC35 record as text
*  --------------------------
*/
static IC35REC *
txt_putic35rec( IC35REC * rec )
{
    static int		lastfileid = -1;
    static int		idx;
    int 		fileid;
    int 		fi, i;
    uchar *		fld;
    size_t		len;

    fileid = FileId( ic35recid( rec ) );
    if ( ic35fname( fileid ) == NULL )
	return NULL;				/* unknown file id	*/
    if ( fileid != lastfileid ) {		/* first/next IC35 file */
	fprintf( outfp, "\nFile \"%s\"\n", ic35fname( fileid ) );
	lastfileid = fileid;
	idx = 0;				/*  reset record index	*/
    } else {					/* same IC35 file	*/
	++idx;					/*  increment rec.index	*/
    }
    get_ic35recdata( rec, NULL, &len );
    fprintf(outfp,"File \"%s\"  Record %3d IC35id=%08lX cflag=%02X length=%d\n",
		  ic35fname(fileid), idx, ic35recid(rec), ic35recchg(rec), len);
    for ( fi = 0; fi < ic35fnflds( fileid ); ++fi ) {
	get_ic35recfld( rec, FILEfld(fileid,fi), &fld, &len );
	fprintf( outfp, "f%d(%d)\t", fi, len );
	if ( len != 0 ) {
	    fprintf( outfp, "\"" );
	    for ( i = 0; i < len; ++i ) {
		if ( isprint( fld[i] ) )
		    fprintf( outfp, "%c", fld[i] );
		else
		    fprintf( outfp, "\\x%02X", (uchar)fld[i] );
		if ( 0 < i && i < len - 1
		  && fld[i-1] == '\r' && fld[i] == '\n' )
		    fprintf( outfp, "\"\n\t\"" );
	    }
	    fprintf( outfp, "\"\n" );
	} else {
	   fprintf( outfp, "<EMPTY>\n" );
	}
    }
    return rec;
}


/* text format operations
*  ----------------------
*/
struct pim_oper	txt_oper = {
			    txt_open,
			    txt_close,
			    NULL,		/* NO rewind	  */
			    NULL,		/* NO getrec	  */
			    NULL,		/* NO getrec_byID */
			    NULL,		/* NO cmpic35rec  */
			    NULL,		/* NO updic35rec  */
	(void*(*)(IC35REC*))txt_putic35rec,
			    NULL,		/* NO delrec	  */
			    NULL,		/* NO recid	  */
			    NULL,		/* NO set_recid   */
			    NULL,		/* NO recstat	  */
			    NULL,		/* NO set_recstat */
			};

