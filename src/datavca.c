/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*/
	static char rcsid[] =
	"$Id: datavca.c,v 1.46 2001/03/02 02:09:59 tsch Rel $";  	/*
*									*
* IC35 synchronize data import/export: vCard,vCalendar format		*
*									*
*************************************************************************
*									*
*	??? is "fixme" mark: sections of code needing fixes		*
*									*
*	rev_dtime		yyyy-mm-ddThh:mm:ss for VCARRD		*
*	mod_dtime		yyyymmddThhmmss for VEVENT,VTODO,memo	*
*	    IC35 records to/from vCard,vCalendar			*
*	ic35addr_to_vcard
*	vcard_to_ic35addr
*	ic35sched_to_vevent
*	vevent_to_ic35sched
*	ic35todo_to_vtodo
*	vtodo_to_ic35todo
*	ic35memo_to_vmemo
*	vmemo_to_ic35memo
*	    vCard,vCal access functions					*
*	_vca_prodid		local: vCal program korganizer,gnomecal
*	_vca_rewind		local: rewind record list using iterator
*	_vca_getnext		local: get next record using iterator
*	vca_open
*	vca_close
*	vca_rewind
*	vca_getrec
*	vca_getrec_byID
*	vca_updic35rec
*	vca_cmpic35rec
*	vca_putic35rec
*	vca_delrec
*	vca_recid
*	vca_set_recid
*	vca_recstat
*	vca_set_recstat
*									*
************************************************************************/

#include <stdio.h>	/* sprintf(), ..	*/
#include <string.h>	/* strcpy(), ..		*/
#include <ctype.h>	/* isdigit(), ..	*/
#include <unistd.h>	/* access()		*/
#include <sys/types.h>	/* size_t, ..		*/
#include <time.h>	/* struct tm, time() ..	*/

#include "util.h"	/* ERR, uchar, ..	*/
#include "vcc.h"	/* VObject, ..		*/
#include "vcutil.h"	/* vCard,vCal utilities */
#include "ic35frec.h"	/* IC35 record fields.. */
#include "dataio.h"
NOTUSED(rcsid);


/* record status values for X-PILOTSTAT */
#define VCA_CLEAN	PIM_CLEAN
#define VCA_DIRTY	PIM_DIRTY
/* vCalendar program ids		*/
#define VCAL_GNOME	1
#define VCAL_KDE	2


static int	_vca_prodid( void );
static void	vca_set_recid( VObject * vobj, ulong recid );
static void	vca_set_recstat( VObject * vobj, int stat );


/* ============================================ */
/*	IC35 Address record to/from vCard	*/
/* ============================================ */
/*
*   field mapping:
*	VCNameProp				preferred
*	  VCGivenNameProp	A_FirstName
*	  VCFamilyNameProp	A_LastName
*	VCFullNameProp				only if VCNameProp absent
*	  first field		A_FirstName
*	  last field		A_LastName
*	VCBirthDateProp		A_BirthDate
*	VCAdrProp
*	  VCStreetAddressProp	A_Street
*	  VCCityProp		A_City
*	  VCRegionProp		A_Region
*	  VCPostalCodeProp	A_ZIP
*	  VCCountryNameProp	A_Country
*	VCTelephoneProp
*	  VCHomeProp		A_TelHome
*	VCTelephoneProp
*	  VCWorkProp		A_TelWork
*	VCTelephoneProp
*	  VCCellularProp	A_TelMobile
*	VCTelephoneProp
*	  VCFaxProp		A_TelFax
*	VCEmailAddressProp
*	  VCInternetProp	A_Email1
*	VCEmailAddressProp
*	  VCInternetProp	A_Email2
*	VCOrgProp
*	  VCOrgNameProp		A_Company
*	VCURLProp		A_URL
*	VCNoteProp		A_Notes
*	  "(def1):<cont>\n"	A_Def1
*	  "(def2):<cont>\n"	A_Def2
*	-/-			A_CategoryID		  default: 0x00
*	VCCategoriesProp	A_Category		  default: "Unfiled"
*	    use first known standard from multiple (Business,Personal,Unfiled)
*	    if single use it, even if non-standard
*/

/* convert IC35 Address record to vCard
*  ------------------------------------
*/
static void
ic35addr_to_vcard( IC35REC * rec, VObject * vcard )
{
    VObject *	vprop;
    char *	fullname, *oldstr, *ic35first, *ic35last;

    ic35first = ic35recfld( rec, A_FirstName );
    ic35last  = ic35recfld( rec, A_LastName  );
    vprop = SetProp( vcard, VCNameProp);
    if ( (fullname = dupStringValue( vcard, VCFullNameProp )) && *fullname ) {
	/*
	* if old vCard (is present and) has fullname VCFullNameProp "FN:"
	* substitute old VCFamilyNameProp in its value with A_FirstName
	* and old VCGivenNameProp in it with A_LastName. no changes to
	* VCFullNameProp will be made if VCGivenNameProp,VCFamilyNameProp
	* are not substrings of it.
	*/
	fullname = dupSubst( oldstr = fullname,
			      StringValue( vprop, VCFamilyNameProp ), ic35last );
	deleteStr( oldstr );
	fullname = dupSubst( oldstr = fullname,
			      StringValue( vprop, VCGivenNameProp ), ic35first );
	deleteStr( oldstr );
    } else {
	/*
	* otherwise (no old vCard or no VCFullNameProp in it) construct
	* VCFullNameProp from A_FirstName,A_LastName separated by space.
	* if either A_FirstName or A_LastName is missing construct from
	* the other one if present.
	*/
	if ( ic35first && *ic35first && ic35last && *ic35last ) {
	    fullname = dupStr( ic35first, strlen(ic35first)+1+strlen(ic35last) );
	    strcat( strcat( fullname, " " ), ic35last );
	} else
	    fullname = dupStr( ic35first && *ic35first ? ic35first :
				  ic35last && *ic35last ? ic35last : "", 0 );
    }
    SetString( vcard, VCFullNameProp, fullname );
    deleteStr( fullname );
    SetString( vprop, VCGivenNameProp, ic35first );
    SetString( vprop, VCFamilyNameProp, ic35last );

    SetString( vcard, VCBirthDateProp, ic35recfld( rec, A_BirthDate ) );

    if ( strlen( ic35recfld( rec, A_Street ) ) != 0
      || strlen( ic35recfld( rec, A_City   ) ) != 0
      || strlen( ic35recfld( rec, A_Region ) ) != 0
      || strlen( ic35recfld( rec, A_ZIP    ) ) != 0
      || strlen( ic35recfld( rec, A_Country ) ) != 0 ) {
	vprop = SetProp( vcard, VCAdrProp );
	SetString( vprop, VCPostalBoxProp,      "" );
	SetString( vprop, VCExtAddressProp,     "" );
	SetString( vprop, VCStreetAddressProp,ic35recfld( rec, A_Street ) );
	SetString( vprop, VCCityProp,         ic35recfld( rec, A_City ) );
	SetString( vprop, VCRegionProp,       ic35recfld( rec, A_Region ) );
	SetString( vprop, VCPostalCodeProp,   ic35recfld( rec, A_ZIP ) );
	SetString( vprop, VCCountryNameProp,  ic35recfld( rec, A_Country ) );
    } else
	DelProp( vcard, VCAdrProp );

    SetTel( vcard, VCHomeProp,     ic35recfld( rec, A_TelHome ) );
    SetTel( vcard, VCWorkProp,     ic35recfld( rec, A_TelWork ) );
    SetTel( vcard, VCCellularProp, ic35recfld( rec, A_TelMobile ) );
    SetTel( vcard, VCFaxProp,      ic35recfld( rec, A_TelFax ) );

    SetEmail( vcard, 1, ic35recfld( rec, A_Email1 ) );
    SetEmail( vcard, 2, ic35recfld( rec, A_Email2 ) );

    if ( strlen( ic35recfld( rec, A_Company ) ) != 0 ) {
	vprop = SetProp( vcard, VCOrgProp );
    	SetString( vprop, VCOrgNameProp, ic35recfld( rec, A_Company ) );
    } else
	DelProp( vcard, VCOrgProp );

    SetString( vcard, VCURLProp, ic35recfld( rec, A_URL ) );

    SetNoteProp( vcard, DEF1PROP, ic35recfld( rec, A_Def1 ) );
    SetNoteProp( vcard, DEF2PROP, ic35recfld( rec, A_Def2 ) );
    SetNotes( vcard, ic35recfld( rec, A_Notes ) );

    SetCategory( vcard, ic35recfld( rec, A_Category ) );
}

/* convert vCard to IC35 address record
*  ------------------------------------
*/
static void
vcard_to_ic35addr( VObject * vcard, IC35REC * rec )
{
    VObject *		vprop;
    VObjectIterator	iter;
    char *		strval;
    char *		pstr;
    const char *	name;
    int 		emailnum;

    if ( (vprop = isAPropertyOf( vcard, VCNameProp )) != NULL ) {
	set_ic35recfld( rec, A_LastName,
			StringValue( vprop, VCFamilyNameProp ) );
	set_ic35recfld( rec, A_FirstName,
			StringValue( vprop, VCGivenNameProp ) );
    } else if ( (strval = StringValue( vcard, VCNameProp )) && *strval ) {
	/* no Name property, parse first,last field from FormattedName */
	if ( (pstr = strrchr( strval, ' ' )) != NULL ) {
	    set_ic35recfld( rec, A_LastName, pstr+1 );
	    pstr = strchr( strval, ' ' );
	    *pstr = '\0';
	    set_ic35recfld( rec, A_FirstName, strval );
	} else
	    set_ic35recfld( rec, A_LastName, strval );
    } else {
	set_ic35recfld( rec, A_LastName, "NoName" );	/* IC35 needs name */
    }
    vprop = isAPropertyOf( vcard, VCAdrProp );
    set_ic35recfld( rec, A_City,
		    StringValue( vprop, VCCityProp ) );
    set_ic35recfld( rec, A_Street,
		    StringValue( vprop, VCStreetAddressProp ) );
    set_ic35recfld( rec, A_Region,
		    StringValue( vprop, VCRegionProp ) );
    set_ic35recfld( rec, A_ZIP,
		    StringValue( vprop, VCPostalCodeProp ) );
    set_ic35recfld( rec, A_Country,
		    StringValue( vprop, VCCountryNameProp ) );
    vprop = isAPropertyOf( vcard, VCOrgProp );
    set_ic35recfld( rec, A_Company,
		    StringValue( vprop, VCOrgNameProp ) );

    emailnum = 0;
    set_ic35recfld( rec, A_TelHome,   "" );
    set_ic35recfld( rec, A_TelWork,   "" );
    set_ic35recfld( rec, A_TelMobile, "" );
    set_ic35recfld( rec, A_TelFax,    "" );
    set_ic35recfld( rec, A_Email1, "" );
    set_ic35recfld( rec, A_Email2, "" );
    initPropIterator( &iter, vcard );
    while ( moreIteration( &iter ) ) {
	vprop = nextVObject( &iter );
	name  = vObjectName( vprop );
	if ( strcasecmp( name, VCTelephoneProp ) == 0 ) {
	    if      ( isAPropertyOf( vprop, VCHomeProp ) )
		set_ic35recfld( rec, A_TelHome,   StringValue( vprop, NULL ) );
	    else if ( isAPropertyOf( vprop, VCWorkProp ) )
		set_ic35recfld( rec, A_TelWork,   StringValue( vprop, NULL ) );
	    else if ( isAPropertyOf( vprop, VCCellularProp ) )
		set_ic35recfld( rec, A_TelMobile, StringValue( vprop, NULL ) );
	    else if ( isAPropertyOf( vprop, VCFaxProp ) )
		set_ic35recfld( rec, A_TelFax,    StringValue( vprop, NULL ) );
	    else if ( strcasecmp( "Business", CategoryValue( vcard ) ) == 0 )
		set_ic35recfld( rec, A_TelWork,   StringValue( vprop, NULL ) );
	    else
		set_ic35recfld( rec, A_TelHome,   StringValue( vprop, NULL ) );
	}
	if ( strcasecmp( name, VCEmailAddressProp ) == 0 )
	    switch ( ++emailnum ) {
	    case 1:
		set_ic35recfld( rec, A_Email1,
				StringValue( vprop, NULL ) );
		break;
	    case 2:
		set_ic35recfld( rec, A_Email2,
				StringValue( vprop, NULL ) );
		break;
	    }
    }

    set_ic35recfld( rec, A_URL, StringValue( vcard, VCURLProp ) );

    set_ic35recfld( rec, A_BirthDate,
		    isodtime_to_ymd( vcard, VCBirthDateProp ) );

    set_ic35recfld( rec, A_Def1, NotePropValue( vcard, DEF1PROP ) );
    set_ic35recfld( rec, A_Def2, NotePropValue( vcard, DEF2PROP ) );
    set_ic35recfld( rec, A_Notes, NotesValue( vcard ) );

    set_ic35recfld( rec, A_CategoryID, "\x00" );
    set_ic35recfld( rec, A_Category, CategoryValue( vcard ) );
}


/* ==================================================== */
/*	IC35 Schedule record to/from vCal:vEvent	*/
/* ==================================================== */
/*
*   field mapping:
*	VCSummaryProp		S_Subject
*	VCDTstartProp
*	  date			S_StartDate
*	  time			S_StartTime
*	VCDTendProp
*	  date			S_EndDate
*	  time			S_EndTime
*	VCRRuleProp
*	  type D,W,MP,MD,YD	S_Alarm_Repeat & RepeatMASK
*	    D  daily		  RepeatDay
*	    W  weekly		  RepeatWeek
*	    MP monthly		  RepeatMonWday
*	    MD monthly		  RepeatMonMday
*	    YD yearly		  RepeatYear
*	  count n		S_RepCount
*	  end-isodtime		S_RepEndDate	    only yyyymmdd, max 20311231
*	VCAAlarmProp		S_Alarm_Repeat & AlarmNoBeep
*	  VCRunTimeProp		S_AlarmBefore		default: AlarmBefNone
*	VCDAlarmProp		S_Alarm_Repeat & AlarmNoLED
*	  VCRunTimeProp		S_AlarmBefore		default: AlarmBefNone
*	  VCRunTimeProp - VCDTstartProp
*	  <  60			  AlarmBefNow
*	  >= 60 * 1		  AlarmBef1min
*	  >= 60 * 5		  AlarmBef5min
*	  >= 60 * 10		  AlarmBef10min
*	  >= 60 * 30		  AlarmBef30min
*	  >= 60 * 60 * 1	  AlarmBef1hour
*	  >= 60 * 60 * 2	  AlarmBef2hour
*	  >= 60 * 60 * 10	  AlarmBef10hour
*	  >= 60 * 60 * 24 * 1	  AlarmBef1day
*	  >= 60 * 60 * 24 * 2	  AlarmBef2day
*	VCDescriptionProp	S_Notes
*	-/-			S_CategoryID		  default: 0x00
*	VCCategoriesProp	S_Category		  default: "Unfiled"
*	    use first known standard from multiple (Business,Personal,Unfiled)
*	    if single use it, even if non-standard
*   special for korganizer:
*	korganizer supports only DALARM
*   special for gnomecal:
*	VCSummaryProp
*	  first line		S_Subject
*	  subsequent lines	S_Notes
*	gnomecal does not support VEVENT.DESCRIPTION, but multiple lines
*	in VEVENT.SUMMARY, which in turn the IC35 does not support.
*	as workaround the first line of VEVENT.SUMMARY is mapped to S_Subject,
*	all subsequent lines are mapped to S_Notes in IC35.
*	VCClassProp		PUBLIC
*	gnomecal crashes on edit if VCClassProp is not in VEVENT record
*   gnomecal,korganizer automagically add:
*	CLASS:PUBLIC		gnomecal need	korganizer
*	PRIORITY:0		gnomecal	korganizer
*	RELATED-TO:0		 -		korganizer
*	SEQUENCE:0		gnomecal	korganizer
*	STATUS:NEEDS ACTION	gnomecal	korganizer
*	TRANSP:0		gnomecal	korganizer
*	X-ORGANIZER:MAILTO:tsch@shs5.ind-hh	korganizer
*/

/* convert IC35 Schedule record to vCal:vEvent
*  -------------------------------------------
*/
static void
ic35sched_to_vevent( IC35REC * rec, VObject * vevent )
{
    VObject *	vprop;
    uchar	alarm_repeat;
    char	isodtime[20];
    char *	notes;

    sprintf( isodtime, "%sT%s",				/* yyyymmddThhmmss */
	      ic35recfld( rec, S_StartDate ), ic35recfld( rec, S_StartTime ) );
    if ( isodtime[13] == ':' ) isodtime[13] = isodtime[14] = '0';
    SetString( vevent, VCDTstartProp, isodtime );
    sprintf( isodtime, "%sT%s",				/* yyyymmddThhmmss */
	      ic35recfld( rec, S_EndDate ), ic35recfld( rec, S_EndTime ) );
    if ( isodtime[13] == '<' ) isodtime[13] = isodtime[14] = '0';
    SetString( vevent, VCDTendProp, isodtime );

    if ( _vca_prodid() == VCAL_GNOME ) {
    	if ( (notes = ic35recfld( rec, S_Notes )) && *notes ) {
	    char *	subj = ic35recfld( rec, S_Subject );
	    char *	buff = dupStr( "", strlen(subj)+2+strlen(notes) );
	    sprintf( buff, "%s\r\n%s", subj, notes );
	    subj = dupSubstCRLF( buff );
	    SetString( vevent, VCSummaryProp, subj );
	    deleteStr( subj );
	    deleteStr( buff );
	} else
	    SetString( vevent, VCSummaryProp, ic35recfld( rec, S_Subject ) );
	if ( ! isAPropertyOf( vevent, VCClassProp ) )
	    SetString( vevent, VCClassProp, "PUBLIC" );
    } else {
	SetString( vevent, VCSummaryProp, ic35recfld( rec, S_Subject ) );
	SetNotes( vevent, ic35recfld( rec, S_Notes ) );
    }

    alarm_repeat = *ic35recfld( rec, S_Alarm_Repeat );
    if ( alarm_repeat & RepeatMASK ) {
	uchar 		repcount = *ic35recfld( rec, S_RepCount );
	char *		repend   =  ic35recfld( rec, S_RepEndDate );
	char		reprule[24];
	struct tm	stm;
	static const char * wdays[] = {
				"SU", "MO", "TU", "WE", "TH", "FR", "SA",
			    };

	memset( &stm, 0, sizeof(stm) );
	sscanf( ic35recfld( rec, S_StartDate ), "%4d%2d%2d",
			    &stm.tm_year, &stm.tm_mon, &stm.tm_mday );
	sscanf( ic35recfld( rec, S_StartTime ), "%2d%2d",
			    &stm.tm_hour, &stm.tm_min );
	stm.tm_year -= 1900;
	stm.tm_mon  -= 1;
	stm.tm_sec = 0;
	stm.tm_isdst = -1;
	(void)mktime( &stm );
	repcount = *ic35recfld( rec, S_RepCount );
	repend   =  ic35recfld( rec, S_RepEndDate );
	*reprule = '\0';
	switch ( alarm_repeat & RepeatMASK ) {
	case RepeatDay:
	    sprintf( reprule, "D%u %sT000000", repcount, repend );
	    break;
	case RepeatWeek:
	    sprintf( reprule, "W%u %s %sT000000",
				repcount, wdays[stm.tm_wday], repend );
	    break;
	case RepeatMonWday:
	    sprintf( reprule, "MP%u %sT000000", repcount, repend );
	    break;
	case RepeatMonMday:
	    sprintf( reprule, "MD%u %sT000000", repcount, repend );
	    break;
	case RepeatYear:
	    sprintf( reprule, "YD%u %sT000000", repcount, repend );
	    break;
	}
	SetString( vevent, VCRRuleProp, reprule );
    } else
	DelProp( vevent, VCRRuleProp );
    if ( *ic35recfld( rec, S_AlarmBefore )
      && (alarm_repeat & (AlarmNoBeep|AlarmNoLED))
	 != (AlarmNoBeep|AlarmNoLED) ) {
	time_t		atime;
	struct tm *	ptm;
	struct tm	atm;
	char		atimestr[16];

	memset( &atm, 0, sizeof(atm) );
	sscanf( ic35recfld( rec, S_StartDate ), "%4d%2d%2d",
				&atm.tm_year, &atm.tm_mon, &atm.tm_mday );
	sscanf( ic35recfld( rec, S_StartTime ), "%2d%2d",
				&atm.tm_hour, &atm.tm_min );
	atm.tm_year -= 1900;
	atm.tm_mon  -= 1;
	atm.tm_isdst = -1;
	atime = mktime( &atm );
	switch ( *ic35recfld( rec, S_AlarmBefore ) ) {
	case AlarmBefNone:
	case AlarmBefNow:	break;
	case AlarmBef1min:	atime -=           60;	break;
	case AlarmBef5min:	atime -=       5 * 60;	break;
	case AlarmBef10min:	atime -=      10 * 60;	break;
	case AlarmBef30min:	atime -=      30 * 60;	break;
	case AlarmBef1hour:	atime -=      60 * 60;	break;
	case AlarmBef2hour:	atime -=  2 * 60 * 60;	break;
	case AlarmBef10hour:	atime -= 10 * 60 * 60;	break;
	case AlarmBef1day:	atime -= 24 * 60 * 60;	break;
	case AlarmBef2day:	atime -= 2 * 24 * 60 * 60; break;
	}
	ptm = localtime( &atime );
	strftime( atimestr, sizeof(atimestr), "%Y%m%dT%H%M%S", ptm );
	if ( ! (alarm_repeat & AlarmNoBeep) && _vca_prodid() != VCAL_KDE ) {
	    vprop = SetProp( vevent, VCAAlarmProp );
	    SetString( vprop, VCRunTimeProp, atimestr );
	    SetString( vprop, VCSnoozeTimeProp, "" );
	    SetString( vprop, VCRepeatCountProp, "1" );
	} else
	    DelProp( vevent, VCAAlarmProp );
	if ( ! (alarm_repeat & AlarmNoLED)
	  || (!(alarm_repeat & AlarmNoBeep) && _vca_prodid() == VCAL_KDE) ) {
	    vprop = SetProp( vevent, VCDAlarmProp );
	    SetString( vprop, VCRunTimeProp, atimestr );
	    SetString( vprop, VCSnoozeTimeProp, "" );
	    SetString( vprop, VCRepeatCountProp, "1" );
	} else
	    DelProp( vevent, VCDAlarmProp );
    } else {
	DelProp( vevent, VCAAlarmProp );
	DelProp( vevent, VCDAlarmProp );
    }
}

/* convert IC35 schedule record to vCal
*  ------------------------------------
*/
static void
vevent_to_ic35sched( VObject * vevent, IC35REC * rec )
{
    VObject *	vprop;
    char *	strval;
    char *	pstr;
    char	alarm_repeat[2];
    char	repeat_count[2];
    char	alarm_before[2];
    time_t	abefsec;

    strval = StringValue( vevent, VCSummaryProp );
    if ( !( strval && *strval ) ) strval = "NoSubject"; /*IC35 needs subject*/
    if ( _vca_prodid() == VCAL_GNOME
      && (pstr = strchr( strval, '\n' )) != NULL ) {
	strval = dupStr( strval, pstr - strval );	/* first line	    */
	set_ic35recfld( rec, S_Subject, strval );	/* to Subject field */
	deleteStr( strval );
	pstr = dupSubstNL( pstr+1 );			/* subsequent lines */
	set_ic35recfld( rec, S_Notes, pstr );		/* to Notes field   */
	deleteStr( pstr );
    } else {
	set_ic35recfld( rec, S_Subject, strval );
	set_ic35recfld( rec, S_Notes, NotesValue( vevent ) );
    }

    set_ic35recfld( rec, S_StartDate,
		    isodtime_to_ymd_or_today( vevent, VCDTstartProp ) );
    set_ic35recfld( rec, S_StartTime,
		    isodtime_to_hms_or_now( vevent, VCDTstartProp ) );
    set_ic35recfld( rec, S_EndDate,
		    isodtime_to_ymd_or_today( vevent, VCDTendProp ) );
    set_ic35recfld( rec, S_EndTime,
		    isodtime_to_hms_or_now( vevent, VCDTendProp ) );

    alarm_repeat[0] = 0;
    alarm_repeat[1] = '\0';
    repeat_count[0] = 0;
    repeat_count[1] = '\0';
    if ( (strval = StringValue( vevent, VCRRuleProp )) && *strval ) {
	  if      ( strncasecmp( strval, "YD", 2 ) == 0 ) {
	      alarm_repeat[0] = (alarm_repeat[0] & RepeatMASK) | RepeatYear;
	      repeat_count[0] = (uchar)atoi( strval+2 );
	  } else if ( strncasecmp( strval, "MD", 2 ) == 0 ) {
	      alarm_repeat[0] = (alarm_repeat[0] & RepeatMASK) | RepeatMonMday;
	      repeat_count[0] = (uchar)atoi( strval+2 );
	  } else if ( strncasecmp( strval, "MP", 2 ) == 0 ) {
	      alarm_repeat[0] = (alarm_repeat[0] & RepeatMASK) | RepeatMonWday;
	      repeat_count[0] = (uchar)atoi( strval+2 );
	  } else if ( strncasecmp( strval, "W",  1 ) == 0 ) {
	      alarm_repeat[0] = (alarm_repeat[0] & RepeatMASK) | RepeatWeek;
	      repeat_count[0] = (uchar)atoi( strval+1 );
	  } else if ( strncasecmp( strval, "D",  1 ) == 0 ) {
	      alarm_repeat[0] = (alarm_repeat[0] & RepeatMASK) | RepeatDay;
	      repeat_count[0] = (uchar)atoi( strval+1 );
	  }
	  if ( (pstr = strrchr( strval, ' ' )) != NULL
	    && strlen( pstr+1 ) >= 8	/* yyyymmdd */
	    && isdigit( pstr[1] ) )
	      set_ic35recfld( rec, S_RepEndDate,
			      isodtstr_to_ymd_or_today( pstr+1 ) );
    }

    alarm_repeat[0] |= (AlarmNoBeep|AlarmNoLED);
    alarm_before[0] = AlarmBefNone;
    alarm_before[1] = '\0';
    abefsec = 0;
    if ( (vprop = isAPropertyOf( vevent, VCAAlarmProp )) != NULL ) {
	alarm_repeat[0] &= ~AlarmNoBeep;
	abefsec = isodtime_to_unixtime( vevent, VCDTstartProp )
		- isodtime_to_unixtime(  vprop, VCRunTimeProp );
    }
    if ( (vprop = isAPropertyOf( vevent, VCDAlarmProp )) != NULL ) {
	alarm_repeat[0] &= ~AlarmNoLED;
	if ( _vca_prodid() == VCAL_KDE )  /* korganizer has only DALARM */
	    alarm_repeat[0] &= ~AlarmNoBeep;
	abefsec = isodtime_to_unixtime( vevent, VCDTstartProp )
		- isodtime_to_unixtime(  vprop, VCRunTimeProp );
    }
    if ( (alarm_repeat[0] & (AlarmNoBeep|AlarmNoLED))
			 != (AlarmNoBeep|AlarmNoLED) ) {
	if ( abefsec <  60		 ) alarm_before[0] = AlarmBefNow;
	if ( abefsec >= 60 * 1		 ) alarm_before[0] = AlarmBef1min;
	if ( abefsec >= 60 * 5		 ) alarm_before[0] = AlarmBef5min;
	if ( abefsec >= 60 * 10		 ) alarm_before[0] = AlarmBef10min;
	if ( abefsec >= 60 * 30		 ) alarm_before[0] = AlarmBef30min;
	if ( abefsec >= 60 * 60 * 1	 ) alarm_before[0] = AlarmBef1hour;
	if ( abefsec >= 60 * 60 * 2	 ) alarm_before[0] = AlarmBef2hour;
	if ( abefsec >= 60 * 60 * 10	 ) alarm_before[0] = AlarmBef10hour;
	if ( abefsec >= 60 * 60 * 24 * 1 ) alarm_before[0] = AlarmBef1day;
	if ( abefsec >= 60 * 60 * 24 * 2 ) alarm_before[0] = AlarmBef2day;
    }
    set_ic35recfld( rec, S_Alarm_Repeat, alarm_repeat );
    set_ic35recfld( rec, S_RepCount,     repeat_count );
    set_ic35recfld( rec, S_AlarmBefore,  alarm_before );

    /* IC35 Schedule does not have Category */
}


/* ==================================================== */
/*	IC35 ToDoList record to/from vCal:vTodo 	*/
/* ==================================================== */
/*
*   field mapping:
*	VCSummaryProp		T_Subject		  default: "ToDo"
*	VCDTstartProp		T_StartDate	yyyymmdd  default: today
*	VCDueProp		T_EndDate	yyyymmdd  default: today
*	VCStatusProp		T_Completed	1 byte	  default: 0x00
*	  NEEDS ACTION		  0x00
*	  COMPLETED		  0x01
*	  other			  0x00		"STATUS:<stat>\r\n" to T_Notes
*	VCPriorityProp		T_Priority	1 byte	  default: 0x01
*	  1,2			  0x02		  highest
*	  3,4,0			  0x01		  normal
*	  5..			  0x00		  lowest
*	VCDescriptionProp	T_Notes			  default: <empty>
*	-/-			T_CategoryID		  default: 0x00
*	VCCategoriesProp	T_Category		  default: "Unfiled"
*	    use first known standard from multiple (Business,Personal,Unfiled)
*	    if single use it, even if non-standard
*   special for KOrganizer:
*	VCDescriptionProp	T_Notes	
*	  "DTSTART:<cont>\r\n"	  T_StartDate
*	  "DUE:<cont>\r\n"	  T_EndDate
*	  "CATEGORIES:<cont>\r\n" T_Category
*	KOrganizer does not support VTODO.DTSTART,DUE,CATEGORIES,
*	as workaround these fields are put into VTODO.DESCRIPTION.
*   special for gnomecal:
*	VCDescriptionProp	T_Notes	
*	  "DTSTART:<cont>\r\n"	  T_StartDate
*	gnomecal does not support VTODO.DTSTART, put into VTODO.DESCRIPTION.
*	VCClassProp		PUBLIC
*	gnomecal crashes on edit if VCClassProp is not in VTODO record
*   gnomecal,korganizer automagically add:
*	CLASS:PUBLIC		gnomecal need	 -
*	SEQUENCE:0		gnomecal	korganizer
*	TRANSP:0		gnomecal	 -
*	X-ORGANIZER:MAILTO:tsch@shs5.ind-hh	korganizer
*/

/* convert IC35 ToDoList record to vCal:vTodo
*  ------------------------------------------
*/
static void
ic35todo_to_vtodo( IC35REC * rec, VObject * vtodo )
{
    char *	prio;
    char	isodtime[20];

    SetString( vtodo, VCSummaryProp, ic35recfld( rec, T_Subject ) );
    SetNotes( vtodo, ic35recfld( rec, T_Notes ) );

    switch ( _vca_prodid() ) {
    case VCAL_KDE:		/* does not support DTSTART,DUE,CATEGORY */
	SetNoteProp( vtodo, VCDueProp,     ic35recfld( rec, T_EndDate ) );
	SetNoteProp( vtodo, VCDTstartProp, ic35recfld( rec, T_StartDate ) );
	SetNoteProp( vtodo, VCCategoriesProp, ic35recfld( rec, T_Category ) );
	break;
    case VCAL_GNOME:		/* does not support DTSTART, needs CLASS */
	if ( ! isAPropertyOf( vtodo, VCClassProp ) )
	    SetString( vtodo, VCClassProp, "PUBLIC" );
	SetNoteProp( vtodo, VCDTstartProp, ic35recfld( rec, T_StartDate ) );
	goto due_category;
    default:
	sprintf( isodtime, "%sT000000", ic35recfld( rec, T_StartDate ) );
	SetString( vtodo, VCDTstartProp, isodtime );
    due_category:
	sprintf( isodtime, "%sT235900", ic35recfld( rec, T_EndDate ) );
	SetString( vtodo, VCDueProp, isodtime );
	SetCategory( vtodo, ic35recfld( rec, T_Category ) );
    }

    SetString( vtodo, VCStatusProp, *ic35recfld( rec, T_Completed ) == 0
					? "NEEDS ACTION" : "COMPLETED" );

    switch ( *ic35recfld( rec, T_Priority ) ) {
    case 0x02:	prio = "1"; break;	/* highest */
    default:
    case 0x01:	prio = "3"; break;	/* normal */
    case 0x00:	prio = "5"; break;	/* lowest */
    }
    SetString( vtodo, VCPriorityProp, prio );
}

/* convert IC35 ToDoList record to vCal
*  ------------------------------------
*/
static void
vtodo_to_ic35todo( VObject * vtodo, IC35REC * rec )
{
    char	*binstr, *strval;
    int 	intval;
    char *	statusnote = NULL;
    char *	dtbeg;
    char *	dtend;

    strval = StringValue( vtodo, VCSummaryProp );
    if ( !( strval && *strval ) ) strval = "NoSubject";	/*IC35 needs subject*/
    set_ic35recfld( rec, T_Subject, strval );

    switch ( _vca_prodid() ) {
    case VCAL_KDE:		/* does not support DTSTART,DUE,CATEGORY */
	dtbeg = dupStr( NotePropValue( vtodo, VCDTstartProp ), 0 );
	dtend = dupStr( NotePropValue( vtodo, VCDueProp ), 0 );
	set_ic35recfld( rec, T_Category,
			NotePropValue( vtodo, VCCategoriesProp ) );
	break;
    case VCAL_GNOME:		/* does not support DTSTART		 */
	dtbeg = dupStr( NotePropValue( vtodo, VCDTstartProp ), 0 );
	goto due_category;
    default:
	dtbeg = dupStr( StringValue( vtodo, VCDTstartProp ), 0 );
    due_category:
	dtend = dupStr( StringValue( vtodo, VCDueProp ), 0 );
	set_ic35recfld( rec, T_Category,
			CategoryValue( vtodo ) );
    }
    set_ic35recfld( rec, T_CategoryID, "\x00" );
    set_ic35recfld( rec, T_StartDate, isodtstr_to_ymd_or_today( dtbeg ) );
    set_ic35recfld( rec, T_EndDate,   isodtstr_to_ymd_or_today( dtend ) );
    deleteStr( dtbeg );
    deleteStr( dtend );

    binstr = "\x00";
    if ( (strval = StringValue( vtodo, VCStatusProp )) && *strval ) {
	if ( strcasecmp( strval, "COMPLETED" ) == 0 )
	    binstr = "\x01";
	else if ( strcasecmp( strval, "NEEDS ACTION" ) != 0
	       && (statusnote = malloc( 255+1 )) != NULL ) {
	    strcpy( statusnote, "STATUS:" );
	    strncat( strval, strval, 255-7-2 );
	    strcat( strval, "\r\n" );
	}
    }
    set_ic35recfld( rec, T_Completed, binstr );

    intval = atoi( StringValue( vtodo, VCPriorityProp ) );
    if      ( intval >= 5 )  binstr = "\x00";	/* lowest  */
    else if ( intval >= 3 )  binstr = "\x01";	/* normal  */
    else if ( intval >= 1 )  binstr = "\x02";	/* highest */
    else   /* intval == 0 */ binstr = "\x01";	/* default: normal */
    set_ic35recfld( rec, T_Priority, binstr );

    if ( statusnote ) {
	strval = NotesValue( vtodo );
	strncat( statusnote, strval, 255 - strlen( statusnote ) );
	set_ic35recfld( rec, T_Notes, statusnote );
	free( statusnote );
    } else
	set_ic35recfld( rec, T_Notes, NotesValue( vtodo ) );
}


/* ==================================================== */
/*	IC35 Memo record to/from vMemo (proprietary) 	*/
/* ==================================================== */
/*
*   field mapping:
*	VCSummaryProp		M_Subject	default: "Memo"
*	VCNoteProp		M_Notes
*	-/-			M_CategoryID	default: 0x00
*	VCCategoriesProp	M_Category	default: "Unfiled"
*	    use first known standard from multiple (Business,Personal,Unfiled)
*	    if single use it, even if non-standard
*/

/* convert IC35 Memo record to (proprietary) vMemo
*  -----------------------------------------------
*/
static void
ic35memo_to_vmemo( IC35REC * rec, VObject * vmemo )
{
    SetString( vmemo, VCSummaryProp, ic35recfld( rec, M_Subject ) );
    SetNotes( vmemo, ic35recfld( rec, M_Notes ) );
    SetCategory( vmemo, ic35recfld( rec, M_Category ) );
}

/* convert IC35 Memo record to (proprietary) vMemo
*  -----------------------------------------------
*/
static void
vmemo_to_ic35memo( VObject * vmemo, IC35REC * rec )
{
    char	*strval;

    strval = StringValue( vmemo, VCSummaryProp );
    if ( !( strval && *strval ) ) strval = "NoSubject";	/*IC35 needs subject*/
    set_ic35recfld( rec, M_Subject, strval );

    set_ic35recfld( rec, M_Notes, NotesValue( vmemo ) );

    set_ic35recfld( rec, M_CategoryID, "\x00" );
    set_ic35recfld( rec, M_Category, CategoryValue( vmemo ) );
}


/* ============================================ */
/*	vCard,vCal access functions		*/
/* ============================================ */
/*
*	vca_open()
*	vca_close()
*	vca_rewind()
*
*	vca_getrec( int fileid )		-> VObject* / NULL
*	vca_getrec_byID( ulong recid ) 		-> VObject* / NULL
*	vca_cmpic35rec( IC35REC*, VObject* )	-> 0:same 1:differ
*	vca_updic35rec( IC35REC*, VObject* )	VObject to IC35REC
*	vca_putic35rec( IC35REC* )		IC35REC to VObject
*	vca_delrec( VObject* )
*
*	vca_recid( VObject* )
*	vca_set_recid( VObject*, ulong )
*	vca_recstat( VObject* )
*	vca_set_recstat( VObject*, int )
*/

struct vcaget {			/* current position in vcalist	*/
    VObject *	    vobj;	/*  current vCard,vCal,vMemo	*/
    bool	    in_vcal;	/*  flag: vcal_iterator in use	*/
    VObjectIterator vcal_iter;	/*  iterator within vCal object */
};

static VObject *	vcalist; /* list of vCard,vCal,vMemo objects	*/
static VObject *	vcal;	 /* vCalendar contains vEvent,vTodo	*/
static struct vcaget	vcaget;	 /* _vca_getrec position in vcalist	*/

static ulong	vca_recid( VObject * vobj );

/* internal: determine vCalender program from PRODID
*  -------------------------------------------------
*	inspect vCalender PRODID for creating program:
*	PRODID:-//GNOME//NONSGML GnomeCalendar//EN
*	PRODID:-//K Desktop Environment//NONSGML KOrganizer//EN
*	some IC35 to/from vCalendar conversions are handled special
*	depending on program.
*/
static int
_vca_prodid( void )
{
    char *	prodid;
    int 	rval;

    rval = 0;
    if ( vcal != NULL
      && (prodid = dupStringValue( vcal, VCProdIdProp )) != NULL ) {
	if ( strstr( prodid, "GnomeCalendar//" ) != NULL )
	    rval = VCAL_GNOME;
	if ( strstr( prodid, "KOrganizer//" ) != NULL )
	    rval = VCAL_KDE;
	deleteStr( prodid );
    }
    return rval;
}

/* internal: rewind vCard,vCal list
*  --------------------------------
*/
static void
_vca_rewind( struct vcaget * vcaget )
{
    vcaget->vobj = NULL;
    vcaget->in_vcal = FALSE;
}
/* internal: get next vCard,vCal record
*  ------------------------------------
*/
static VObject *
_vca_getnext( struct vcaget * vcaget )
{
    VObject *	vcal;

    if ( vcaget == NULL )
	return NULL;
    if ( vcaget->vobj == NULL )
	vcaget->vobj = vcalist;
    else if ( ! vcaget->in_vcal )
	vcaget->vobj = nextVObjectInList( vcaget->vobj );
    while ( vcaget->vobj != NULL ) {
	switch ( vca_type( vcaget->vobj ) ) {
	case VCARD:
	case VMEMO:
	    return vcaget->vobj;
	case VCAL:
	    if ( ! vcaget->in_vcal ) {
		initPropIterator( &vcaget->vcal_iter, vcaget->vobj );
		vcaget->in_vcal = TRUE;
	    }
	    while ( moreIteration( &vcaget->vcal_iter ) ) {
		vcal = nextVObject( &vcaget->vcal_iter );
		switch ( vca_type( vcal ) ) {
		case VEVENT:
		case VTODO:
		    return vcal;
		}
	    }
	    vcaget->in_vcal = FALSE;
	    break;
	}
	vcaget->vobj = nextVObjectInList( vcaget->vobj );
    }
    return NULL;
}

/* open vCard,vCal file(s)
*  -----------------------
*	if file(s) exist, read into internal vObject-tree 'vcalist'
*	complain if open file(s) failed.
* ???	check if output file creatable in dirname(filename)
*	rewind so that _vca_getrec() gets first record in 'vcalist'
*/
static char *	iomode;
static char *	addr_fname;
static char *	vcal_fname;
static char *	memo_fname;
static int	addr_CRLF;	/* CRLF / NL mode per PIMfile	*/
static int	vcal_CRLF;	/*  from Parse_MIME_FromFile()	*/
static int	memo_CRLF;	/*   calls in vca_open()	*/

/* mimeErrorHandler for Parse_MIME_FromFile()
*/
static int	_vca_errok;
static char *	_vca_fname;

static void
_vca_errmsg( char * msg )
{
    error( "vCard/vCal \"%s\": %s", _vca_fname ? _vca_fname : "", msg );
    _vca_errok = ERR;
}

/* check and read vCard/vCal from filepointer
*/
static int
_vca_readfp( FILE * fp, char * fname, VObject ** pvlist, int * pcrlf )
{
    int 	chr;

    LPRINTF(( L_INFO, "vca_open: read %s ..", fname ));
    *pvlist = NULL;
    *pcrlf  = 1;
    do {
	if ( (chr = fgetc( fp )) == EOF )	/* empty file or only CR/LF */
	    return OK;				/*  is accepted OK	    */
    } while ( chr == '\r' || chr == '\n' );
    rewind( fp );

    registerMimeErrorHandler( _vca_errmsg );
    _vca_errok = OK;
    _vca_fname = fname;
    *pvlist = Parse_MIME_FromFile( fp );
    *pcrlf  = CRLFmode();		/* CRLF/NL input from Parse_MIME */
    return _vca_errok;			/* ok / error from _vca_errmsg() */
}

/* open,check+read,close vCard/vCal file
*/
static int
_vca_readfile( char * fname, VObject ** pvlist, int * pcrlf )
{
    FILE *	fp;
    int 	rval;

    *pvlist = NULL;
    *pcrlf  = 1;
    if ( !( fname && *fname ) )		/* no filename specified	*/
	return OK;

    if ( (fp = fopen( fname, "r" )) == NULL ) {
	if ( access( fname, F_OK ) == 0 )
	    return ERR;			/* file exists, but not readable */
	else
	    return OK;			/* non-existing file is OK	 */
    }
    rval = _vca_readfp( fp, fname, pvlist, pcrlf );
    fclose( fp );
    return rval;
}

/* read-in vCard,vCal file(s)
*/
static int
vca_open( char * mode, char * addrfname, char * vcalfname, char * memofname )
{
    VObject *	vlist;
    VObject *	vobj;
    int 	rval;

    LPRINTF(( L_INFO, "vca_open(%s,%s,%s)",
			addrfname ? addrfname : "NULL",
			vcalfname ? addrfname : "NULL",
			vcalfname ? addrfname : "NULL" ));
    iomode = mode;			/* note input/output for close	*/
    addr_fname = addrfname;		/* note filenames for close	*/
    vcal_fname = vcalfname;
    memo_fname = memofname;
    vcalist = vcal = NULL;		/* init vCard,vCal record lists	*/
    addr_CRLF = vcal_CRLF = memo_CRLF = 1; /* init CRLF mode default ON */
    if ( iomode[0] != 'r' )
	return OK;			/* open for output only		*/

    if ( addrfname && strcmp( addrfname, "-" ) == 0	/* using stdin	*/
      && !( vcalfname && *vcalfname )		     /* is allowed if	*/
      && !( memofname && *memofname ) ) 	  /* only one filename	*/
	rval = _vca_readfp( stdin, "(stdin)", &vcalist, &addr_CRLF );
    else
	rval = _vca_readfile( addrfname, &vcalist, &addr_CRLF );
    if ( rval != OK )			/* open/parse vCard file failed */
	return ERR;

    rval = _vca_readfile( vcalfname, &vcal, &vcal_CRLF );
    if ( rval != OK )			/* open/parse vCal file failed	*/
	return ERR;
    if ( vcal != NULL ) 		/* vCal object parsed from file */
	addList( &vcalist, vcal );	  /* add vCal object to vcalist */
    else				/* vCal object in addr file	*/
	for ( vobj = vcalist; vobj != NULL; vobj = nextVObjectInList( vobj ) )
	    if ( vca_type( vobj ) == VCAL ) {
		vcal = vobj;		  /* setup pointer to vCal obj	*/
		break;
	    }

    rval = _vca_readfile( memofname, &vlist, &memo_CRLF );
    if ( rval != OK )			/* open/parse VMEMO file failed */
	return ERR;
    while ( vlist != NULL ) {
	vobj = vlist;
	vlist = nextVObjectInList( vlist );
	addList( &vcalist, vobj );	/* add memo records to vcalist	*/
    }

    _vca_rewind( &vcaget );	/* rewind for _vca_getrec() get first record */
    LPRINTF(( L_INFO, "vca_open: done" ));
    return OK;
}

/* close vCard,vCal file(s)
*  ------------------------
*	flush internal vObject-tree to vCard,vCal file(s)
*	if file(s) exist, rename file(s) for backup
*	static filename(s) were setup by pim_open()
*/
static void
vca_close( void )
{
    FILE *	addrfp = NULL;
    FILE *	vcalfp = NULL;
    FILE *	memofp = NULL;
    VObject *	vobj;

    if ( iomode[0] == 'w' || iomode[1] == '+' ) {
	addrfp = backup_and_openwr( addr_fname );
	vcalfp = backup_and_openwr( vcal_fname );
	if ( vcalfp == NULL ) { vcalfp = addrfp; vcal_CRLF = addr_CRLF; }
	memofp = backup_and_openwr( memo_fname );
	if ( memofp == NULL ) { memofp = vcalfp; memo_CRLF = memo_CRLF; }
    }
    LPRINTF(( L_INFO, "vca_close: %s",
			addrfp && vcalist ? "write .." : "no output" ));
    for ( vobj = vcalist; vobj != NULL; vobj = nextVObjectInList( vobj ) ) {
	FILE *		fp = NULL;
	int		crlf = 0;

	switch ( vca_type( vobj ) ) {
	case VCARD: fp = addrfp; crlf = addr_CRLF; break;
	case VCAL:  fp = vcalfp; crlf = vcal_CRLF; break;
	case VMEMO: fp = memofp; crlf = memo_CRLF; break;
	}
	if ( fp != NULL ) {
	    setCRLFmode( crlf );
	    writeVObject( fp, vobj );
	}
    }
    cleanVObjects( vcalist );
    vcalist = vcal = NULL;	/* all vCard,vCal,vMemo objects were removed */
    _vca_rewind( &vcaget );	/* sanity for vca_getrec()		     */

    if ( addrfp != NULL && addrfp != stdout )
	fclose( addrfp );
    if ( vcalfp && vcalfp != addrfp && vcalfp != stdout )
	fclose( vcalfp );
    if ( memofp && memofp != vcalfp && memofp != stdout )
	fclose( memofp );
    LPRINTF(( L_INFO, "vca_close: done" ));
}

/* rewind vCard,vCal list
*  ----------------------
*/
static void
vca_rewind( void )
{
    _vca_rewind( &vcaget );
}

/* get vCard,vCal record for fileid
*  --------------------------------
*	fileid specifies which type of record to get:
*	FILEADDR  VCARD
*	FILESCHED VCAL:VEVENT
*	FILETODO  VCAL:VTODO
*	FILEMEMO  VMEMO
*   returns:
*	VObject*	next record
*	NULL		no more records
*/
static VObject *
vca_getrec( int fileid )
{
    VObject *	vobj;
    int 	wanttype;

    switch ( fileid ) {
    case FILEADDR:  wanttype = VCARD;	break;
    case FILESCHED: wanttype = VEVENT;	break;
    case FILETODO:  wanttype = VTODO;	break;
    case FILEMEMO:  wanttype = VMEMO;	break;
    case FILE_ANY:  wanttype = 0;	break;
    default:
	return NULL;			/* unknown fileid */
    }
    while ( (vobj = _vca_getnext( &vcaget )) ) {
	if ( wanttype == 0		/* next record with any fileid	*/
	  || vca_type( vobj ) == wanttype )
	    return vobj;
    }
    return NULL;
}
/* get vCard,vCal record by record-ID
*  ----------------------------------
*/
static VObject *
vca_getrec_byID( ulong recid )
{
    VObject *		vobj;
    struct vcaget	tmpget;

    _vca_rewind( &tmpget );
    while ( (vobj = _vca_getnext( &tmpget )) )
	if ( vca_recid( vobj ) == recid )
	    return vobj;
    return NULL;
}

/* update IC35 record with vCard,vCal record
*  -----------------------------------------
*/
static IC35REC *
vca_updic35rec( IC35REC * ic35rec, VObject * vobj )
{
    int 	fid;
    void     (* vobj_to_ic35 )(VObject*,IC35REC*);

    switch ( vca_type( vobj ) ) {
    case VCARD:  vobj_to_ic35 =  vcard_to_ic35addr;  fid = FILEADDR;  break;
    case VEVENT: vobj_to_ic35 = vevent_to_ic35sched; fid = FILESCHED; break;
    case VTODO:  vobj_to_ic35 =  vtodo_to_ic35todo;  fid = FILETODO;  break;
    case VMEMO:  vobj_to_ic35 =  vmemo_to_ic35memo;  fid = FILEMEMO;  break;
    default:
	return NULL;
    }
    if ( ic35rec == NULL
      && (ic35rec = new_ic35rec()) == NULL )
	return NULL;
		/* must set fileid,recid first to specify number of fields */
    set_ic35recid( ic35rec, FileRecId( fid, RecId(vca_recid( vobj )) ) );
    (*vobj_to_ic35)( vobj, ic35rec );
    return ic35rec;
}

/* compare IC35 record with vCard,vCal record
*  ------------------------------------------
*/
static int
vca_cmpic35rec( IC35REC * ic35rec, VObject * vobj )
{
    IC35REC *	pimic35;
    int		rval;

    if ( ic35rec == NULL && vobj == NULL )
	return 0;
    if ( ic35rec == NULL || vobj == NULL )
	return -1;

    if ( (pimic35 = new_ic35rec()) == NULL )
	return -1;
    vca_updic35rec( pimic35, vobj );
    rval = cmp_ic35rec( ic35rec, pimic35 );
    del_ic35rec( pimic35 );
    return rval;
}

/* put IC35 record to (new) vCard,vCal record
*  ------------------------------------------
*/
static VObject *
vca_putic35rec( IC35REC * ic35rec )
{
    VObject *	oldvobj;
    VObject *	vobj = NULL;
    VObject *	locvcal;
    const char *id;
    void     (* ic35_to_vobj )(IC35REC*,VObject*);

    switch ( FileId( ic35recid( ic35rec ) ) ) {
    case FILEADDR:  ic35_to_vobj = ic35addr_to_vcard;   id = VCCardProp;  break;
    case FILEMEMO:  ic35_to_vobj = ic35memo_to_vmemo;   id = VCMemoProp;  break;
    case FILESCHED: ic35_to_vobj = ic35sched_to_vevent; id = VCEventProp; break;
    case FILETODO:  ic35_to_vobj = ic35todo_to_vtodo;   id = VCTodoProp;  break;
    default:
	return NULL;
    }
    if ( (vobj = oldvobj = vca_getrec_byID( ic35recid( ic35rec ) )) == NULL
      && (vobj = newVObject( id )) == NULL )
	return NULL;
    clr_vobjdirty();
    (*ic35_to_vobj)( ic35rec, vobj );
    SetModtimeIfdirty( vobj );
    vca_set_recid( vobj, ic35recid( ic35rec ) );
    vca_set_recstat( vobj, VCA_CLEAN );
    if ( oldvobj != NULL )
	return vobj;

    switch ( vca_type( vobj ) ) {
    case VCARD:
    case VMEMO:
	addList( &vcalist, vobj );
	break;
    case VEVENT:
    case VTODO:
	/* create vCalendar header of not yet present */
	if ( (locvcal = vcal) == NULL )
	    locvcal = newVObject( VCCalProp );
	if ( isAPropertyOf( locvcal, VCProdIdProp ) == NULL )
	    addPropValue(   locvcal, VCProdIdProp,
				    "-//IC35//NONSGML IC35Calendar//EN" );
				  /* -//GNOME//NONSGML GnomeCalendar//EN */
		     /* -//K Desktop Environment//NONSGML KOrganizer//EN */
	if ( isAPropertyOf( locvcal, VCVersionProp ) == NULL )
	    addPropValue(   locvcal, VCVersionProp, "1.0" );
	if ( isAPropertyOf( locvcal, VCTimeZoneProp ) == NULL
	  || isAPropertyOf( locvcal, VCDCreatedProp ) == NULL ) {
	    extern long timezone;
	    time_t	tnow;
	    struct tm *	ptm;
	    char	isodtime[24];
	    char	tzoffset[8];
	    long	tzmineastUTC;
	    tnow = time( NULL ); ptm = localtime( &tnow );
	    tzmineastUTC = -timezone / 60;
	    strftime( isodtime, sizeof(isodtime), "%Y-%m-%dT%T", ptm );
	    sprintf( tzoffset, "%+03ld:%02ld",
				tzmineastUTC/60, tzmineastUTC%60 );
	    if ( isAPropertyOf( locvcal, VCTimeZoneProp ) == NULL )
		addPropValue(   locvcal, VCTimeZoneProp, tzoffset );
	    if ( isAPropertyOf( locvcal, VCDCreatedProp ) == NULL )
		addPropValue(   locvcal, VCDCreatedProp, isodtime );
	}
	if ( vcal == NULL )
	    addList( &vcalist, vcal = locvcal );
	addVObjectProp( vcal, vobj );
	break;
    }
    return vobj;
}

/* delete vCard,vCal record
*  ------------------------
*/
static void
vca_delrec( VObject * vobj )
{
    switch ( vca_type( vobj ) ) {
    case VCARD:
    case VMEMO:
	cleanVObject( delList( &vcalist, vobj ) );
	break;
    case VEVENT:
    case VTODO:
	cleanVObject( delProp( vcal, vobj ) );
	break;
    }
}

/* get,set record-ID
*  -----------------
*/
static ulong
vca_recid( VObject * vobj )
{
    long	recid;

    if ( (recid = LongValue( vobj, XPilotIdProp )) == -1 )
	return 0;
    return (ulong)recid;
}
static void
vca_set_recid( VObject * vobj, ulong recid )
{
    char *	IC35id = "IC35id";
    char *	ouid;
    char	nuid[7+8+1];

    ouid = StringValue( vobj, VCUniqueStringProp );
    if ( !( ouid && *ouid )				/* not present yet   */
      || strncmp( ouid, IC35id, strlen( IC35id ) ) == 0 ) { /* or IC35 recID */
	sprintf( nuid, "IC35id-%08lX", recid );
	SetString( vobj, VCUniqueStringProp, nuid ); /* korganizer wants UID */
    }
    SetLong( vobj, XPilotIdProp, recid );
}
/* get,set record-changeflag
*  -------------------------
*	gnomecard (from gnome-pim-1.2.0) does not maintain X-PILOTSTAT !
*	as workaround the last-revised-time VCARD.REV is compared against
*	the previous time of sync with IC35 'oldic35dt()' to obtain the
*	record-status CLEAN or DIRTY.
*/
static int
vca_recstat( VObject * vobj )
{
    long	stat;

    if ( (stat = LongValue( vobj, XPilotStatusProp )) == -1 )
	return VCA_DIRTY;
    if ( stat <= 0
      && vca_type( vobj ) == VCARD
      && oldic35dt() != 0
      && oldic35dt() < isodtime_to_unixtime( vobj, VCLastRevisedProp ) )
	return VCA_DIRTY;
    return (int)stat;
}
static void
vca_set_recstat( VObject * vobj, int stat )
{
    SetLong( vobj, XPilotStatusProp, (long)stat );
}

/* vCard,vCal format operations
*  ----------------------------
*/
struct pim_oper	vca_oper = {
				vca_open,
				vca_close,
				vca_rewind,
		 (void*(*)(int))vca_getrec,
	       (void*(*)(ulong))vca_getrec_byID,
	(int(*)(IC35REC*,void*))vca_cmpic35rec,
   (IC35REC*(*)(IC35REC*,void*))vca_updic35rec,
	    (void*(*)(IC35REC*))vca_putic35rec,
		(void(*)(void*))vca_delrec,
	       (ulong(*)(void*))vca_recid,
	  (void(*)(void*,ulong))vca_set_recid,
		 (int(*)(void*))vca_recstat,
	    (void(*)(void*,int))vca_set_recstat,
			};

