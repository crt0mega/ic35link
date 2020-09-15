/************************************************************************
* Copyright (C) 2001 Thomas Schulz					*
*									*
*	$Id: mgrproto.h,v 1.8 2001/11/20 23:08:35 thosch Exp $  	*
*									*
* header for IC35 manager protocol					*
*									*
************************************************************************/
#ifndef _MGRPROTO_H
#define	_MGRPROTO_H	1

#include "util.h"	/* uchar	*/


/* manager command bytes  */
#define MCMDdisconn	(uchar)0x01	/* disconnect			*/
#define MCMDreset	(uchar)0x09	/* reset communication		*/
#define MCMDident	(uchar)0x10	/* identify: get "DCS_SDK" 00	*/
#define MCMDbackup	(uchar)0x13	/* backup database		*/
#define MCMDrestinit	(uchar)0x14	/* restore database command-1	*/
#define MCMDmmcard	(uchar)0x15	/* start MMCard transaction	*/
#define MCMDinfo	(uchar)0x18	/* get backup info "0128"	*/
#define MCMDinit	(uchar)0x50	/* status command-2, no response*/
#define MCMDposack	(uchar)0x60	/* positive acknowledge 	*/
#define MCMDnegack	(uchar)0x62	/* negative acknowledge 	*/
#define MCMDrestdata	(uchar)0x70	/* restore database command-2	*/
#define MCMDstatus	(uchar)0xFF	/* get 16400 byte status block	*/
/* manager response bytes */
#define MRSPgotcmd	(uchar)0x90	/* IC35 got command		*/
#define MRSPgotack	(uchar)0xA0	/* IC35 got ack/nak		*/
#define MRSPrestdata	(uchar)0xC0	/* restore database response-2	*/
#define MRSPgotlen	(uchar)0xE0	/* IC35/PC got length		*/

/* MMCard file attributes (from IC35 SDK Mmc.h) */
#define MMCattrReadOnly 	0x01
#define MMCattrHidden		0x02
#define MMCattrSystemFile	0x04
#define MMCattrVolumeLabel	0x08
#define MMCattrDirectory	0x10
#define MMCattrArchive		0x20
/* MMCard file open modes (see IC35 SDK MMc.h)	*/
#define MMCopenexist		0x0001	/* open existing file for read,write */
#define MMCcreatrunc		0x0000	/* create new truncate existing file */


#pragma pack(1)
typedef struct _file_info {	/* directory entry (see IC35 SDK Mmc.h) */
    char	FileName[8+1];
    char	ExtName[3+1];
    uchar	Attribute;
    ushort	ModifyTime;
    ushort	ModifyDate;
    ushort	Reserved;
    ulong	FileSize;
}			FILE_INFO;
typedef struct _file_iden {	/* file identifier (see IC35 SDK Mmc.h) */
    ushort	Sector;
    ushort	Cluster;
    ushort	SectorOffset;
    ushort	DirItemOffset;
    ushort	StartCluster;
    ulong	FilePointer;
    ulong	FileSize;
    uchar	CacheNo;
    ushort	CurCluster;
    ushort	CurClusterNo;
    ushort	CurSectorNo;
    ushort	Reserved;
}			FILE_IDEN;
#pragma pack()
#define FIDENSZ 	sizeof(FILE_IDEN)


int Mcmdrsp( uchar cmd, uchar rsp );	/* send cmd, get+check rsp	*/

int Msendblk( uchar * data, size_t dlen );  /* send block, ack/retry	*/
int Mrecvblk( uchar * buff, size_t blen );  /* receive block, ack/retry */

long MMCgetstatus( int mmcnum );
long MMCgetlabel( int mmcnum,  char * plabel );

long MMCdiropen( char * dirpath,  uchar * fdstat );
long MMCdircreate( char * dirpath,  uchar * fdstat );
long MMCdirgetlen( uchar * fdstat,  ushort * pndent );
long MMCdirread(   uchar * fdstat, ushort index,  FILE_INFO * pdirent );
long MMCdirclose(  uchar * fdstat );

long MMCfiledel( char * filepath );
long MMCfileopen( char * filepath, ushort mode,  uchar * fdstat, ulong * psize );
long MMCfilestat(  uchar * fdstat,  FILE_INFO * pdirent );
long MMCfileread(  uchar * fdstat, uchar * buff, ushort blen,  ushort * prlen );
long MMCfilewrite( uchar * fdstat, uchar * data, ushort dlen );
long MMCfileclose( uchar * fdstat );

#endif /*_MGRPROTO_H*/
