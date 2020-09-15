/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*
*	$Id: ic35frec.h,v 1.5 2000/12/23 01:09:26 tsch Rel $  		*
*									*
* header for IC35 record access						*
*									*
************************************************************************/
#ifndef _IC35REC_H
#define	_IC35REC_H	1

#include "util.h"	/* uchar, ..	*/


/* IC35 database files	*/
#define FILEADDR	0x05
#define FILEMEMO	0x06
#define FILETODO	0x07
#define FILESCHED	0x08
#define FILE_ANY	0

#define MAXFLDS	21		    /* max.number of fields in IC35 record    */
#define MAXRLEN	(MAXFLDS * (1+255)) /* #fields [Addresses] * (len + fieldlen) */

/* record-ids consist of file-id and record-id	*/
#define FileId(frid)		( ((ulong)(frid) & 0xFF000000) >> 24 )
#define RecId(frid)		( ((ulong)(frid) & 0x00FFFFFF) )
#define FileRecId(fid,rid)	( (((ulong)(fid) << 24) & 0xFF000000) \
				| ( (ulong)(rid)        & 0x00FFFFFF) )
/* field-ids consist of file-id and field-index */
#define FldFile(fldid)		( ((fldid) & 0xFF00) >> 8 )
#define FldIdx(fldid)		(  (fldid) & 0x00FF	  )

/* IC35 record fields	*/
#define FILEfld(fileid,index)	( (fileid)<<8 | (index) )
/* record fields "Addresses"	*/
#define ADDRfld(index)	FILEfld( FILEADDR, index )
#define A_LastName	ADDRfld(  0 )	/* max.  50 chars */
#define A_FirstName	ADDRfld(  1 )	/* max.  50 chars */
#define A_Company	ADDRfld(  2 )	/* max. 128 chars */
#define A_TelHome	ADDRfld(  3 )	/* max.  48 chars */
#define A_TelWork	ADDRfld(  4 )	/* max.  48 chars */
#define A_TelMobile	ADDRfld(  5 )	/* max.  48 chars */
#define A_TelFax	ADDRfld(  6 )	/* max.  48 chars */
#define A_Street	ADDRfld(  7 )	/* max. 128 chars */
#define A_City		ADDRfld(  8 )	/* max.  60 chars */
#define A_ZIP		ADDRfld(  9 )	/* max.  10 chars */
#define A_Region	ADDRfld( 10 )	/* max.  40 chars */
#define A_Country	ADDRfld( 11 )	/* max.  15 chars */
#define A_Email1	ADDRfld( 12 )	/* max.  80 chars */
#define A_Email2	ADDRfld( 13 )	/* max.  80 chars */
#define A_URL		ADDRfld( 14 )	/* max. 128 chars */
#define A_BirthDate	ADDRfld( 15 )	/* max.  10 chars */
#define A_Notes		ADDRfld( 16 )	/* max. 255 chars */
#define A_CategoryID	ADDRfld( 17 )	/* 1 Byte */
#define A_Def1		ADDRfld( 18 )	/* max. 128 chars */
#define A_Def2		ADDRfld( 19 )	/* max. 128 chars */
#define A_Category	ADDRfld( 20 )	/* max.   8 chars */
/* record fields "Schedule"	*/
#define SCHEDfld(index)	FILEfld( FILESCHED, index )
#define S_Subject	SCHEDfld( 0 )	/* max.  60 chars */
#define S_StartDate	SCHEDfld( 1 )	/* 8 yyyymmdd */
#define S_StartTime	SCHEDfld( 2 )	/* 6 hhmmss   */
#define S_EndTime	SCHEDfld( 3 )	/* 6 hhmmss   */
#define S_AlarmBefore	SCHEDfld( 4 )	/* 1 Byte */
#define   AlarmBefNone		0x00
#define   AlarmBefNow		0x01
#define   AlarmBef1min		0x02
#define   AlarmBef5min		0x03
#define   AlarmBef10min		0x04
#define   AlarmBef30min		0x05
#define   AlarmBef1hour		0x06
#define   AlarmBef2hour		0x07
#define   AlarmBef10hour	0x08
#define   AlarmBef1day		0x09
#define   AlarmBef2day		0x0A
#define S_Notes		SCHEDfld( 5 )	/* max. 255 chars */
#define S_Alarm_Repeat	SCHEDfld( 6 )	/* 1 Byte */
#define   AlarmNoLED		0x80
#define   AlarmNoBeep		0x40
#define	  RepeatMASK		0x0F
#define     RepeatNone		0x00
#define     RepeatDay		0x01
#define     RepeatWeek		0x02
#define     RepeatMonWday	0x03
#define     RepeatYear		0x04
#define     RepeatMonMday	0x05
#define S_EndDate	SCHEDfld( 7 )	/* 8 yyyymmdd */
#define S_RepEndDate	SCHEDfld( 8 )	/* 6 hhmmss   */
#define S_RepCount	SCHEDfld( 9 )	/* 1 Byte */
/* record fields "To Do List"	*/
#define TODOfld(index)	FILEfld( FILETODO, index )
#define T_StartDate	TODOfld( 0 )	/* 8 yyyymmdd */
#define T_EndDate	TODOfld( 1 )	/* 8 yyyymmdd */
#define T_Completed	TODOfld( 2 )	/* 1 Byte */
#define T_Priority	TODOfld( 3 )	/* 1 Byte */
#define T_Subject	TODOfld( 4 )	/* max.  60 chars */
#define T_Notes		TODOfld( 5 )	/* max. 255 chars */
#define T_CategoryID	TODOfld( 6 )	/* 1 Byte */
#define T_Category	TODOfld( 7 )	/* max.   8 chars */
/* record fields "Memo"		*/
#define MEMOfld(index)	FILEfld( FILEMEMO, index )
#define M_Subject	MEMOfld( 0 )	/* max.  60 chars */
#define M_Notes		MEMOfld( 1 )	/* max. 255 chars */
#define M_CategoryID	MEMOfld( 2 )	/* 1 Byte */
#define M_Category	MEMOfld( 3 )	/* max.   8 chars */

/* IC35 record change flags	*/
#define IC35_NEW	0x80	/* new record			*/
#define IC35_MOD	0x40	/* modified			*/
#define IC35_DEL	0x20	/* deleted (no record data)	*/
#define IC35_CLEAN	0x00	/* unchanged			*/


typedef struct ic35rec	IC35REC;


char *	ic35fname( int fileid );
int	ic35fnflds( int fileid );

IC35REC * new_ic35rec( void );
void	  del_ic35rec( IC35REC * rec );

void	set_ic35recid( IC35REC * rec, ulong recid );
ulong	ic35recid( IC35REC * rec );
void	set_ic35recchg( IC35REC * rec, uchar chgflag );
uchar	ic35recchg( IC35REC * rec );

void	set_ic35recdata( IC35REC * rec, uchar * data, size_t dlen );
void	get_ic35recdata( IC35REC * rec, uchar ** pdata, size_t * pdlen );

void	set_ic35recfld( IC35REC * rec, int fldid, char * data );
void	get_ic35recfld( IC35REC * rec, int fldid, uchar** pfld, size_t * plen );
char *	ic35recfld( IC35REC * rec, int fldid );

int	cmp_ic35rec( IC35REC * rec1, IC35REC * rec2 );

#endif /*_IC35REC_H*/
