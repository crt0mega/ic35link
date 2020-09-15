/************************************************************************
* Copyright (C) 2001 Thomas Schulz					*
*									*
*	$Id: mgrtrans.h,v 1.7 2001/11/20 23:08:35 thosch Exp $  		*
*									*
* header for IC35 manager transactions					*
*									*
************************************************************************/
#ifndef _MGRTRANS_H
#define	_MGRTRANS_H	1

#include <time.h>	/* time_t		*/

#include "util.h"	/* uchar,ulong, ..	*/
#include "mgrproto.h"	/* MMCattr*, open modes	*/


typedef struct mmcdir	MMCDIR;
typedef struct mmcdirent {	/* exported MMC directory entry */
    char	name[8+1+3+1];	/*  filename.ext		*/
    uchar	attr;		/*  file attributes		*/
    ulong	tstamp; 	/*  timestamp (DOS-format)	*/
    ulong	size;		/*  size in bytes		*/
}			MMCDIRENT;

typedef struct mmcfile	MMCFILE;


int mconnect( char * devname, char * statfname );
int mdisconnect( void );

int mmc_status( int mmcnum );
int mmc_label( int mmcnum, char label[11+1] );

MMCDIR *    mmc_opendir( char * dirpath );
MMCDIR *    mmc_createdir( char * dirpath );
MMCDIRENT * mmc_readdir(  MMCDIR * dirp );
int	    mmc_closedir( MMCDIR * dirp );
char *	    mmctstampstr( ulong tstamp );
time_t	    mmctstampunixtime( ulong tstamp );

int         mmc_delfile( char * filepath );
MMCFILE *   mmc_openfile( char * filepath, ushort mode );
MMCDIRENT * mmc_statfile( MMCFILE * fp );
int         mmc_readfile( MMCFILE * fp, uchar * buff, size_t blen );
int         mmc_writefile( MMCFILE * fp, uchar * data, size_t dlen );
int         mmc_closefile( MMCFILE * fp );

int readdatabase( char * fname );
int writedatabase( char * fname );

#endif /*_MGRTRANS_H*/
