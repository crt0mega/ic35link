/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*
*	$Id: syntrans.h,v 1.10 2000/12/21 11:05:52 tsch Rel $  	*
*									*
* header for IC35 synchronize transactions				*
*									*
************************************************************************/
#ifndef _SYNTRANS_H
#define	_SYNTRANS_H	1

#include <sys/types.h>	/* size_t	*/

#include "util.h"	/* uchar, ..	*/
#include "ic35frec.h"	/* IC35REC	*/


int connect( char * devname, char * passwd, char * rdtime );
int disconnect( void );

int ReadSysInfo( char * infobuff );
int WriteSysInfo( char * infodata );
int category( char * name );

int open_file( char * fname );
int get_flen( int fd );
int get_mod_flen( int fd );
int read_id_frec( int fd, ulong rid, IC35REC * rec );
int read_frec( int fd, int idx, IC35REC * rec );
int read_mod_frec( int fd, IC35REC * rec );
int write_frec( int fd, IC35REC * rec );
int update_frec( int fd, IC35REC * rec );
int delete_frec( int fd, ulong recid );
int commit_frec( int fd, ulong recid );
int close_file( int fd );

#endif /*_SYNTRANS_H*/
