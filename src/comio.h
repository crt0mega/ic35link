/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*
*	$Id: comio.h,v 1.7 2001/03/03 16:30:45 tsch Rel $  	*
*									*
* header for IC35 serial communication					*
*									*
************************************************************************/
#ifndef _COMIO_H
#define	_COMIO_H	1

#include <sys/types.h>	/* size_t		*/

#include "util.h"	/* uchar, NO_LOGSIM	*/


#ifdef	NO_LOGSIM
#define COM_SIMINIT(arg)
#else
#define COM_SIMINIT(arg)	com_siminit arg
#endif


void com_siminit( char * s_fname );		/* init comm.simulation	  */
int  com_settimeout( int msec );		/* set timeout [msec]	  */
int  com_init( char * devname );		/* initialize comm.device */
void com_waitnice( int inc );			/* for nice() in com_sendw*/
int  com_sendw( uchar * data, size_t dlen );	/* send data with waiting */
int  com_send( uchar * data, size_t dlen );	/* send data to comm.dev  */
int  com_recv( uchar * buff, size_t blen );	/* receive from comm.dev  */
void com_exit( void );				/* close comm.device	  */

#ifdef	__STRICT_ANSI__
void	usleep( unsigned long usec );
#endif

#endif /*_COMIO_H*/
