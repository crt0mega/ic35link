/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*
*	$Id: vcutil.h,v 1.44 2000/12/27 22:26:52 tsch Rel $  		*
*									*
* header for IC35 data import/export: vCard,vCalendar utilities		*
*									*
************************************************************************/
#ifndef _VCUTIL_H
#define	_VCUTIL_H	1

#include "vcc.h"	/* VObject, ..	*/


/* vCard,vCal record types		*/
#define VCARD		1
#define VMEMO		2
#define VCAL 		3
#define  VEVENT 	4
#define  VTODO		5
/* markers for (def.) in IC35 address	*/
#define DEF1PROP	"(def1)"
#define DEF2PROP	"(def2)"


int vca_type( VObject * vobj );

char * dupSubst( char * str, char * old, char * new );
char * dupSubstNL( char * src );
char * dupSubstCRLF( char * src );

void clr_vobjdirty( void );
void SetModtimeIfdirty( VObject * vobj );

VObject * SetProp( VObject * vobj, const char * id );
void      DelProp( VObject * vobj, const char * id );
VObject * SetString( VObject * vobj, const char * id, char * str );
char *    dupStringValue( VObject * vobj, const char * id );
char *    StringValue( VObject * vobj, const char * id );

void      SetLong( VObject * vobj, const char * id, long value );
long      LongValue( VObject * vobj, const char * id );

void      SetNotes( VObject * vobj, char * value );
void      SetNoteProp( VObject * vobj, const char * id, char * value );
char *    NotesValue( VObject * vobj );
char *    NotePropValue( VObject * vobj, const char * id );

VObject * SetCategory( VObject * vobj, char * newcategory );
char *    CategoryValue( VObject * vobj );

VObject * SetTel( VObject * vobj, const char * type, char * str );
VObject * SetEmail( VObject * vobj, int num, char * str );

char * isodtstr_to_ymd_or_today( char * dtstr );
time_t isodtime_to_unixtime( VObject * vobj, const char * id );
char * isodtime_to_ymd( VObject * vobj, const char * id );
char * isodtime_to_ymd_or_today( VObject * vobj, const char * id );
char * isodtime_to_hms_or_now( VObject * vobj, const char * id );

#endif /*_VCUTIL_H*/
