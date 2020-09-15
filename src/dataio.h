/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*
*	$Id: dataio.h,v 1.12 2000/12/26 01:25:22 tsch Rel $  	*
*									*
* header for IC35 data import/export					*
*									*
************************************************************************/
#ifndef _DATAIO_H
#define	_DATAIO_H	1

#include <stdio.h>	/* FILE		*/
#include <sys/types.h>	/* size_t	*/

#include "vcc.h"	/* VObject, ..	*/
#include "ic35frec.h"	/* IC35REC	*/
#include "util.h"	/* uchar, ..	*/


/* PIM file format ids	*/
#define PIM_TXT	1	/* plain text		*/
#define PIM_BIN 2	/* raw binary data	*/
#define PIM_VCA 3	/* vCard,vCalendar,Memo */
/* PIM record status	*/
#define PIM_CLEAN	0
#define PIM_DIRTY	1
#define PIM_DEL 	3


struct pim_oper {	/* table of record format specific functions	*/
    int 	(*open)( char *, char *, char *, char * );
    void 	(*close)( void );
    void 	(*rewind)( void );
    void * 	(*getrec)( int );
    void *	(*getrec_byID)( ulong );
    int 	(*cmpic35rec)( IC35REC *, void * );	/* IC35 == PIM */
    IC35REC *	(*updic35rec)( IC35REC *, void * );	/* IC35 <- PIM */
    void *	(*putic35rec)( IC35REC * );		/* IC35 -> PIM */
    void	(*delrec)( void * );
    ulong	(*recid)( void * );
    void	(*set_recid)( void *, ulong );
    int 	(*recstat)( void * );
    void	(*set_recstat)( void *, int );
};


FILE * backup_and_openwr( char * fname );

int  set_pim_format( char * format );
int  pim_format( void );

void   clr_newic35dt( void );
void   get_newic35dt( char * dtbuf );
time_t newic35dt( void );
void   set_oldic35dt( char * dtime );
time_t oldic35dt( void );

int  pim_open( char * addrfname, char * vcalfname, char * memofname );
int  pim_openinp( char * format, char * fname );
int  pim_openout( char * format, char * fname );
void pim_close( void );
void pim_rewind( void );

void *	  pim_getrec( int fileid );
void *	  pim_getrec_byID( ulong recid );
void *	  pim_putic35rec( IC35REC * ic35rec );
IC35REC * pim_updic35rec( IC35REC * ic35rec, void * pimrec );
int	  pim_cmpic35rec( IC35REC * ic35rec, void * pimrec );
void	  pim_putrec( void * pimrec );
void	  pim_delrec( void * pimrec );

ulong pim_recid( void * pimrec );
void  pim_set_recid( void * pimrec, ulong recid );
int   pim_recstat( void * pimrec );
void  pim_set_recstat( void * pimrec, int stat );

#endif /*_DATAIO_H*/
