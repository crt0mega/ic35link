/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*
*	$Id: synproto.h,v 1.3 2000/12/03 07:50:44 tsch Rel $  	*
*									*
* header for IC35 synchronize protocol					*
*									*
************************************************************************/
#ifndef _SYNPROTO_H
#define	_SYNPROTO_H	1

#include "util.h"	/* ushort	*/


/* Level-4 commands,responses	*/
#define CMDident	0x1000
#define CMDpassword	0x0300
#define RSPpasswdOK	0x0101
#define CMDgetpower	0x0301
#define RSPpowerOK	0x0301
#define CMDcategory	0x0302
#define RSPcategOK	0x0001
#define CMDgetdtime	0x0200
#define CMDsetdtime	0x0201
#define CMDfopen	0x0002
#define CMDfclose	0x0003
#define CMDfdelrec	0x0102		/* delete record		*/
#define CMDfgetlen	0x0103		/* get total number of records	  */
#define CMDfgetmlen	0x0104		/* get number of modified records */
#define CMDfgetirec	0x0105		/* read record by recID		*/
#define CMDfgetrec	0x0106		/* read record by index		*/
#define CMDfgetmrec	0x0107		/* read next modified record	*/
#define CMDfgetmore	0x83
#define CMDfputrec	0x0108		/* write record, gets new recID	*/
#define CMDfupdrec	0x0109		/* update record, keeps recID	*/
#define CMDfputmore	0x90
#define CMDfclrchg	0x010A		/* commit record		*/
#define CMDdisconn	0x81


int sendcmd( ushort cmd, ... ); 	/* encode command and send	*/
int recvrsp( ushort cmd, ... );		/* receive response and decode	*/

#endif /*_SYNPROTO_H*/
