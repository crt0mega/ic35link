/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*/
	static char rcsid[] =
	"$Id: vcutil.c,v 1.44 2000/12/28 02:26:31 tsch Rel $";  	/*
*									*
* IC35 synchronize data import/export: vCard,vCalendar utilities	*
*									*
*************************************************************************
*									*
*	??? is "fixme" mark: sections of code needing fixes		*
*									*
*	vca_type		determine vCard,vCal record type	*
*	dupSubst		substitute in string, dupStr()'d result *
*	dupSubstNL		translate NL to CRLF, dupStr()'d result *
*	dupSubstCRLF		translate CRLF to NL, dupStr()'d result *
*	    utilities for IC35 to/from vCard,vCal			*
*	clr_vobjdirty		clear vCard,vCal record modified status
*	SetModtimeIfdirty	modified date+time to vCard,vCal record
*	_ChangeString		set property string, note if changed
*	SetProp
*	SetString
*	StringValue
*	SetLong			set property long integer value
*	LongValue
*	_NoteId			NOTE/DESCRIPTION for vCard/vCal
*	_NotePropsLen
*	SetNotes
*	NotesValue
*	SetNoteProp
*	NotePropValue
*	_is_stdCategory		standard categories Personal,Business..
*	SetCategory
*	CategoryValue
*	SetTel			set telephone HOME/WORK/CELL/FAX
*	SetEmail		set Email1,Email2
*	    utilities for date+time conversion				*
*	isodtstr_to_unixtime
*	isodtstr_to_ymd_or_today
*	isodtime_to_unixtime
*	isodtime_to_ymd
*	isodtime_to_ymd_or_today
*	isodtime_to_hms_or_now
*									*
************************************************************************/

#include <stdio.h>	/* sprintf(), ..	*/
#include <stdlib.h>	/* atol()		*/
#include <string.h>	/* strcpy(), ..		*/
#include <ctype.h>	/* isprint(), ..	*/
#include <sys/types.h>	/* size_t, ..		*/
#include <time.h>	/* struct tm, time() ..	*/

#include "util.h"	/* bool			*/
#include "dataio.h"	/* newic35dt()		*/
#include "vcc.h"	/* VObject, ..		*/
#include "vcutil.h"
NOTUSED(rcsid);


/* map vCard,vCal record name to numeric type
*  ------------------------------------------
*/
int
vca_type( VObject * vobj )
{
    const char *	name;

    if ( vobj == NULL )
	return 0;
    name = vObjectName( vobj );
    if ( strcasecmp( name, VCCardProp  ) == 0 ) return VCARD;
    if ( strcasecmp( name, VCMemoProp  ) == 0 ) return VMEMO;
    if ( strcasecmp( name, VCCalProp   ) == 0 ) return VCAL;
    if ( strcasecmp( name, VCEventProp ) == 0 ) return VEVENT;
    if ( strcasecmp( name, VCTodoProp  ) == 0 ) return VTODO;
    return 0;
}

/* string substitutions
*  --------------------
*/
		/* substitutes 'new' for sub-string 'old' in 'str'	*/
char *		/* and returns dupStr()'sd resulting string		*/
dupSubst( char * str, char * old, char * new )
{	/* substitutes 'new' for sub-string 'old' in 'str'	*/
    char	*match, *nstr;
    char	*pdst, *psrc;

    if ( str == NULL )
	return NULL;				/* marginal ..   */
    if ( ! (old && *old && new )		/* .. parameters */
      || (match = strstr( str, old )) == NULL )	/* or no match   */
	return dupStr( str, 0 );
    nstr = dupStr( "", strlen(str) + strlen(new) - strlen(old) );
    pdst = nstr; psrc = str;
    while ( *psrc )
	if ( psrc == match ) {
	    strcpy( pdst, new );
	    psrc += strlen( old );
	    pdst += strlen( new );
	} else
	    *pdst++ = *psrc++;
    return nstr;
}
char *		/* substitute vCard,vCal NewLine with IC35 CRLF 	*/
dupSubstNL( char * src )
{
    char	*psrc, *dst, *pdst;
    size_t	len;

    len = strlen( src );
    for ( psrc = src; *psrc; ++psrc )
	if ( *psrc == '\n' )
	    ++len;
    pdst = dst = dupStr( "", len );
    psrc = src, pdst = dst;
    while ( *psrc ) {
	if ( *psrc == '\n'
	  && !( psrc > src && *(psrc-1) == '\r' ) )
	    *pdst++ = '\r';
	*pdst++ = *psrc++;
    }
    *pdst = '\0';
    return dst;
}
char *		/* substitute IC35 CRLF with vCard,vCal NewLine		*/
dupSubstCRLF( char * src )
{

    char	*dst, *pdst;

    dst = pdst = dupStr( src, 0 );
    while ( *src )
	if ( (*pdst = *src++) != '\r' && *src != '\n' )
	    ++pdst;
    *pdst = '\0';
    return dst;
}


/* ============================================ */
/*	utilities for IC35 to/from vCard,vCal	*/
/* ============================================ */

static bool	_vobj_dirty;	/* VObject changed in ic35xxx_to_vyyy() */
static char	ic35fld[511+1]; /* field buffer used by SetXxx()	*/

/* clear vCard,vCal modified status
*  --------------------------------
*/
void
clr_vobjdirty( void )
{
    _vobj_dirty = FALSE;
}

/* revised date+time for vCard,vCal records
*  ----------------------------------------
*/
static char *
_iso_ic35dt( const char * format )
{
    time_t	ic35dt;
    struct tm * ptm;
    static char isodtime[24];

    ic35dt = newic35dt();
    ptm = localtime( &ic35dt );
    strftime( isodtime, sizeof(isodtime), format, ptm );
    return isodtime;
}
void
SetModtimeIfdirty( VObject * vobj )
{
    if ( ! _vobj_dirty )
	return;
    switch ( vca_type( vobj ) ) {
    case VCARD: 		/* yyyy-mm-ddThh:mm:ss for VCARRD	*/
	SetString( vobj, VCLastRevisedProp, _iso_ic35dt("%Y-%m-%dT%H:%M:%S") );
	break;
    case VEVENT:
    case VTODO:			/* also create-time for VEVENT,VTODO	*/
	if ( ! isAPropertyOf( vobj, VCDCreatedProp ) )
	    SetString( vobj, VCDCreatedProp, _iso_ic35dt("%Y%m%dT%H%M%S") );
	/* fall through */
    case VMEMO: 		/* yyyymmddThhmmss for VEVENT,VTODO,memo */
	SetString( vobj, VCLastModifiedProp, _iso_ic35dt("%Y%m%dT%H%M%S") );
	break;
    }
}

/* set,get property string value
*  -----------------------------
*	handle VObject value types StringZ, UStringZ, Integer, Long
*	determine need for QUOTED-PRINTABLE and CHARSET
*	note if property / string changed in _vobj_dirty
*/
static void	/* set property string, check and note if changed	*/
_ChangeString( VObject * vobj, char * str )
{
    const char * oldstr;

    switch ( vObjectValueType( vobj ) ) {
    case VCVT_STRINGZ:
	oldstr = dupStr( vObjectStringZValue( vobj ), 0 );
	break;
    case VCVT_USTRINGZ:
	oldstr = fakeCString( vObjectUStringZValue( vobj ) );
	break;
    default:
	oldstr = NULL;
    }
    if ( oldstr == NULL || strcmp( oldstr, str ) != 0 ) {
	_vobj_dirty = TRUE;
	setVObjectStringZValue( vobj, str );
    }
    deleteStr( oldstr );
}
VObject *	/* add property if not yet there			*/
SetProp( VObject * vobj, const char * id )
{
    VObject *	vprop;

    if ( (vprop = isAPropertyOf( vobj, id )) )
	return vprop;
    _vobj_dirty = TRUE;
    return addProp( vobj, id );
}
void		/* delete property					*/
DelProp( VObject * vobj, const char * id )
{
    VObject *	vprop;

    if ( vobj && id && *id
      && (vprop = isAPropertyOf( vobj, id )) ) {
	vprop = delProp( vobj, vprop );
	cleanVObject( vprop );
	_vobj_dirty = TRUE;
    }
}
VObject *	/* set string, check if need QUOTED-PRINTABLE or CHARSET */
SetString( VObject * vobj, const char * id, char * str )
{
    VObject *	vprop;
    char *	pstr;
    bool	prop_charset;
    bool	prop_quoted;
    VObject *	vpropchars;

    if ( !( vobj && id && *id ) )
	return NULL;
    if ( !( str && *str ) ) {
	DelProp( vobj, id );
	return NULL;
    }
    if ( (vprop = isAPropertyOf( vobj, id )) == NULL )
	vprop = addProp( vobj, id );
    prop_quoted = prop_charset = FALSE;
    for ( pstr = str; *pstr; ++pstr ) {
	if ( *pstr & 0x80 )
	    prop_charset = TRUE;
	if ( ! isprint( *pstr ) )
	    prop_quoted = TRUE;
    }
    if ( prop_quoted || prop_charset ) {
	/*
	* single-field property like VCARD:FN needs charset,quoted property
	* attached to the property 'vprop'
	* multi-field sub-property like VCARD:ADR:VCCityProp needs charset
	* prop attached to the toplevel property 'vobj'
	* multi-field property must not use QUOTED-PRINTABLE due to problem
	* with vcc.y parser. fixing the parser would not help, because other
	* programs using the vCard,vCal output do not have the fixed parser.
	*/
	switch ( vca_type( vobj ) ) {
	case VCARD:
	case VMEMO:
	case VEVENT:
	case VTODO:
	    vpropchars = vprop; 		/* single-field property */
	    break;
	default:
	    vpropchars = vobj;			/* multi-field sub-property */
	}
	if ( prop_quoted			/* QUOTED-PRINTABLE must not */
	  && vpropchars != vobj )		/*  be used with multi-field */
	    SetProp( vpropchars, VCQuotedPrintableProp );
	if ( prop_charset )
	    SetString( vpropchars, VCCharacterSetProp, "ISO-8859-1" );
    }
    _ChangeString( vprop, str );
    return vprop;
}
char *			/* dupStr()'d property's string value  or  NULL	*/
dupStringValue( VObject * vobj, const char * id )
{
    VObject *	vprop;
    char	vtext[1+10+1];

    if ( vobj == NULL )
	return NULL;
    if ( id && *id ) {
	if ( (vprop = isAPropertyOf( vobj, id )) == NULL )
	    return NULL;
    } else
	vprop = vobj;
    switch ( vObjectValueType( vprop ) ) {
    case VCVT_STRINGZ:
	return dupStr( vObjectStringZValue( vprop ), 0 );
    case VCVT_USTRINGZ:
	return fakeCString( vObjectUStringZValue( vprop ) );
    case VCVT_UINT:
	sprintf( vtext, "%+010d", vObjectIntegerValue( vprop ) );
	return dupStr( vtext, 0 );
    case VCVT_ULONG:
	sprintf( vtext, "%+010ld", vObjectLongValue( vprop ) );
	return dupStr( vtext, 0 );
    default:
	return NULL;
    }
}
char *			/* property's string value  or  ""		*/
StringValue( VObject * vobj, const char * id )
{
    char *	strval;

    if ( (strval = dupStringValue( vobj, id )) == NULL )
	return "";
    strncat( strcpy( ic35fld, "" ), strval, sizeof(ic35fld)-1 );
    deleteStr( strval );
    return ic35fld;
}

/* set,get property long integer value
*  -----------------------------------
*/
void
SetLong( VObject * vobj, const char * id, long value )
{
    VObject *	vprop;

    if ( (vprop = isAPropertyOf( vobj, id )) == NULL )
	vprop = addProp( vobj, id );
    setVObjectLongValue( vprop, value );
}
long			/* property's long value  or  -1 if unknown	*/
LongValue( VObject * vobj, const char * id )
{
    VObject *	vprop;
    char *	strval;
    long	value;

    if ( !( vobj && id && *id )
      || (vprop = isAPropertyOf( vobj, id )) == NULL )
	return -1;
    switch ( vObjectValueType( vprop ) ) {
    case VCVT_STRINGZ:
	return atol( vObjectStringZValue( vprop ) );
    case VCVT_USTRINGZ:
	strval = fakeCString( vObjectUStringZValue( vprop ) );
	value = atol( strval );
	deleteStr( strval );
	return value;
    case VCVT_UINT:
	return vObjectIntegerValue( vprop );
    case VCVT_ULONG:
	return vObjectLongValue( vprop );
    }
    return -1;
}

/* set,get VObject NOTE/DESCRIPTION
*  --------------------------------
*	some properties are not supported by e.g. korganizer, gnomecal,
*	as workaround they are kept is marked lines in NOTE/DESCRIPTION.
*/
static const char *
_NoteId( VObject * vobj )
{
    switch( vca_type( vobj ) ) {
    case VCARD:
    case VMEMO: return VCNoteProp;
    case VEVENT:
    case VTODO: return VCDescriptionProp;
    }
    return NULL;
}
static size_t
_NotePropsLen( char * str, int vtype )
{
    static struct {
	int		type;
	const char *	id;
    }		proptab[] = {
		    { VCARD,	DEF1PROP	 },
		    { VCARD,	DEF2PROP	 },
		    { VTODO,	VCDTstartProp	 },
		    { VTODO,	VCDueProp	 },
		    { VTODO,	VCCategoriesProp },
		    {	0,	NULL		 }
		},
		*ptab;
    char *	pline;
    size_t	lid;

    for ( pline = str; pline; pline = strchr( pline, '\n' ) ) {
	if ( *pline == '\n' )
	    ++pline;
	for ( ptab = proptab; ptab->id != NULL; ++ptab )
	    if ( ptab->type == vtype ) {
		lid = strlen( ptab->id );
		if ( strncmp( pline, ptab->id, lid ) == 0
		  && *(pline+lid) == ':' )
		    break;
	    }
	if ( ptab->id == NULL )
	    break;
    }
    return pline ? pline - str : 0;
}
void
SetNotes( VObject * vobj, char * value )
{
	/* preserve property values stored in NOTE,DESCRIPTION */
	/* translate CRLF to NL to avoid confusing korganizer  */
    char *	onote;
    char *	nnote;
    size_t	lprops;
    const char *id;

    if ( (id = _NoteId( vobj )) == NULL )
	return;
    value = dupSubstCRLF( value ? value : "" );
    if ( (onote = StringValue( vobj, id )) != NULL && *onote ) {
	lprops = _NotePropsLen( onote, vca_type( vobj ) );
	nnote = dupStr( "", lprops + strlen(value) );
	strcat( strncat( strcpy( nnote, "" ), onote, lprops ), value );
	deleteStr( value );
    } else {
	nnote = value;
    }
    SetString( vobj, id, nnote );
    deleteStr( nnote );
}
char *
NotesValue( VObject * vobj )
{
	/* skip property values stored in NOTE,DESCRIPTION */
	/* translate NL to CRLF to avoid confusing IC35    */
    char *	note;
    size_t	lprops;

    if ( (note = StringValue( vobj, _NoteId( vobj ) )) == NULL
      || strlen( note ) == 0 )
	return "";
    lprops = _NotePropsLen( note, vca_type( vobj ) );
    note = dupSubstNL( note+lprops );
    strcat( strcpy( ic35fld, "" ), note );
    deleteStr( note );
    return ic35fld;
}
void
SetNoteProp( VObject * vobj, const char * id, char * value )
{
	/* replace or prepend property value in NOTE/DESCRIPTION */
	/* as marked line "<id>:<value>\n"			 */
    char	*onote, *oprop, *oldstr;
    char	*nnote, *nprop;
    const char	*noteid;

    if ( !((noteid = _NoteId( vobj )) && id && *id ) )
	return;
    onote = dupSubstNL( StringValue( vobj, noteid ) );
    if ( value && *value ) {
	nprop = dupStr( "", strlen(id)+1+strlen(value)+2 );
	sprintf( nprop, "%s:%s\r\n", id, value );
    } else
	nprop = dupStr( "", 0 );
    if ( (oldstr = NotePropValue( vobj, id )) != NULL && *oldstr ) {
	oprop = dupStr( "", strlen(id)+1+strlen(oldstr)+2 );
	sprintf( oprop, "%s:%s\r\n", id, oldstr );
	nnote = dupSubst( onote, oprop, nprop );
	deleteStr( oprop );
    } else {
	nnote = dupStr( "", strlen(nprop)+strlen(onote) );
	strcat( strcpy( nnote, nprop ), onote );
    }
    deleteStr( onote );
    deleteStr( nprop );
    nnote = dupSubstCRLF( oldstr = nnote ); deleteStr( oldstr );
    SetString( vobj, noteid, nnote );
    deleteStr( nnote );
}
char *
NotePropValue( VObject * vobj, const char * id )
{
	/* retrieve property value stored in NOTE/DESCRIPTION	*/
	/* from marked line "<id>:<value>\n"			*/
    char *	note;
    char *	linetag;
    char *	pprop;
    char *	ptr;

    if ( (note = StringValue( vobj, _NoteId( vobj ) )) == NULL
      || strlen( note ) == 0 ) {
	return "";
    }
    sprintf( ptr = dupStr( "", strlen(note)+1 ), "\n%s", note );
    note = dupSubstNL( ptr );
    deleteStr( ptr );
    sprintf( linetag = dupStr( "", strlen(id)+3 ), "\r\n%s:", id );
    if ( (pprop = strstr( note, linetag )) != NULL ) {
	pprop += strlen( linetag );
	if ( (ptr = strstr( pprop, "\r\n" )) != NULL )
	    *ptr = '\0';
	strncat( strcpy( ic35fld, "" ), pprop, sizeof(ic35fld) );
    }
    deleteStr( linetag );
    deleteStr( note );
    return pprop ? ic35fld : "";
}

/* categories
*  ----------
*/
static char *
_is_stdCategory( char * category )
{
    char *	Address  = "Address";
    char *	Business = "Business";
    char *	Personal = "Personal";
    char *	Unfiled  = "Unfiled";

    if      ( strcasecmp( category, Address  ) == 0 )
	return Address;
    else if ( strcasecmp( category, Business ) == 0 )
	return Business;
    else if ( strcasecmp( category, Personal ) == 0 )
	return Personal;
    else if ( strcasecmp( category, Unfiled  ) == 0 )
	return Unfiled;
    return NULL;
}
VObject *
SetCategory( VObject * vobj, char * newcategory )
{
    char *	categories;
    char *	oldcategories;
    char *	category;
    VObject *	vprop;

    if ( (oldcategories = StringValue( vobj, VCCategoriesProp )) == NULL
      || strlen( oldcategories ) == 0		/* no categories  or	*/
      || strchr( oldcategories, ';' ) == NULL )	/*  single category	*/
	return SetString( vobj, VCCategoriesProp, newcategory );
    categories = dupStr( oldcategories, strlen( oldcategories )
				  + 1 + strlen( newcategory ) );
    for ( category = strtok( categories, ";" );
	  category != NULL; category = strtok( NULL, ";" ) ) {
	if ( strcmp( category, newcategory ) == 0 )
	    return isAPropertyOf( vobj, VCCategoriesProp );
	if ( _is_stdCategory( category ) )
	    break;
    }
    if ( category != NULL ) {		/* replace standard category	*/
	category = dupStr( category, 0 );
	deleteStr( categories );
	categories = dupSubst( oldcategories, category, newcategory );
	deleteStr( category );
    } else				/* else prepend new category	*/
	sprintf( categories, "%s;%s", newcategory, oldcategories );
    vprop = SetString( vobj, VCCategoriesProp, categories );
    deleteStr( categories );
    return vprop;
}
char *			/* single, first standard  or  "Unfiled"	*/
CategoryValue( VObject * vobj )
{
    char *	Unfiled = "Unfiled";
    char *	categories;
    char *	category;
    char *	firstcategory;
    char *	firststdcategory;

    if ( (categories = StringValue( vobj, VCCategoriesProp )) == NULL
      || strlen( categories ) == 0 )
	return Unfiled;
    firstcategory = firststdcategory = NULL;
    for ( category = strtok( categories, ";" );
	  category; category = strtok( NULL, ";" ) ) {
	if ( firstcategory == NULL )
	    firstcategory = dupStr( category, 0 );
	if ( firststdcategory == NULL )
	    firststdcategory = _is_stdCategory( category );
    }
    if ( (category = firststdcategory) == NULL ) {
	if ( firstcategory )
	    category = strncat( strcpy( ic35fld, "" ),
				firstcategory, sizeof(ic35fld)-1 );
	else
	    category = Unfiled;
    }
    deleteStr( firstcategory );
    return category;
}

/* telephone numbers
*  -----------------
*/
VObject *
SetTel( VObject * vobj, const char * type, char * str )
{
    VObject *		vprop;
    VObjectIterator	iter;

    initPropIterator( &iter, vobj );
    while ( moreIteration( &iter ) ) {
	vprop = nextVObject( &iter );
	if ( strcmp( vObjectName( vprop ), VCTelephoneProp ) == 0
	  && isAPropertyOf( vprop, type ) ) {
	    if ( str && *str ) {
		_ChangeString( vprop, str );
		return vprop;
	    } else {
		cleanVObject( delProp( vobj, vprop ) );
		_vobj_dirty = TRUE;
		return NULL;
	    }
	}
    }
    if ( !( str && *str ) )
	return NULL;
    _vobj_dirty = TRUE;
    vprop = addPropValue( vobj, VCTelephoneProp, str );
    addProp( vprop, type );
    return vprop;
}

/* email addresses
*  ---------------
*/
VObject *
SetEmail( VObject * vobj, int num, char * str )
{
    VObject *		vprop;
    VObjectIterator	iter;
    int			index;

    index = 0;
    initPropIterator( &iter, vobj );
    while ( moreIteration( &iter ) ) {
	vprop = nextVObject( &iter );
	if ( strcmp( vObjectName( vprop ), VCEmailAddressProp ) == 0
	  && ++index == num ) {
	    if ( str && *str ) {
		_ChangeString( vprop, str );
		return vprop;
	    } else {
		cleanVObject( delProp( vobj, vprop ) );
		_vobj_dirty = TRUE;
		return NULL;
	    }
	}
    }
    if ( !( str && *str ) )
	return NULL;
    _vobj_dirty = TRUE;
    vprop = addPropValue( vobj, VCEmailAddressProp, str );
    addProp( vprop, VCInternetProp );
    return vprop;
}


/* ============================================ */
/*	ISO date+time conversions		*/
/* ============================================ */

static time_t		/* convert ISO date+time to Unix-time		*/
isodtstr_to_unixtime( char * dtstr )
{
    char *	format;
    struct tm	dtm;
    time_t	dtime;

    if ( !( dtstr && *dtstr ) )
	return -1;

    if ( strspn( dtstr, "0123456789" ) == 8 )
	format = "%4d"      "%2d"      "%2d%*[^0-9]%2d"      "%2d"      "%2d";
    else
	format = "%4d%*[^0-9]%2d%*[^0-9]%2d%*[^0-9]%2d%*[^0-9]%2d%*[^0-9]%2d";
    dtime = time( NULL );
    memcpy( &dtm, localtime( &dtime ), sizeof(dtm) );	/* default: today,now */
    sscanf( dtstr, format, &dtm.tm_year, &dtm.tm_mon, &dtm.tm_mday,
			   &dtm.tm_hour, &dtm.tm_min, &dtm.tm_sec );
    dtm.tm_year -= 1900;
    dtm.tm_mon  -= 1;
    dtm.tm_isdst = -1;
    dtime = mktime( &dtm );
/*??? ymdhms_from_isodtime() does not respect timezone/Zulu in isodtime ???*/

    return dtime;
}
char *
isodtstr_to_ymd_or_today( char * dtstr )
{
    static char	ymd[8+1];
    time_t	utsec;

    if ( (utsec = isodtstr_to_unixtime( dtstr )) == -1 )
	utsec = time( NULL );
    strftime( ymd, sizeof(ymd), "%Y%m%d", localtime( &utsec ) );
    return ymd;
}
time_t			/* convert ISO date+time to Unix-time		*/
isodtime_to_unixtime( VObject * vobj, const char * id )
{
    VObject *	vprop;
    char *	dtstr;
    time_t	dtime;

    if ( (vprop = isAPropertyOf( vobj, id )) == NULL )
	return -1;
    dtstr = fakeCString( vObjectUStringZValue( vprop ) );
    dtime = isodtstr_to_unixtime( dtstr );
    deleteStr( dtstr );
    return dtime;
}
char *
isodtime_to_ymd( VObject * vobj, const char * id )
{
    static char	ymd[8+1];
    time_t	utsec;

    if ( (utsec = isodtime_to_unixtime( vobj, id )) == -1 )
	return "";
    strftime( ymd, sizeof(ymd), "%Y%m%d", localtime( &utsec ) );
    return ymd;
}
char *
isodtime_to_ymd_or_today( VObject * vobj, const char * id )
{
    static char	ymd[8+1];
    time_t	utsec;

    if ( (utsec = isodtime_to_unixtime( vobj, id )) == -1 )
	utsec = time( NULL );
    strftime( ymd, sizeof(ymd), "%Y%m%d", localtime( &utsec ) );
    return ymd;
}
char *
isodtime_to_hms_or_now( VObject * vobj, const char * id )
{
    static char	hms[6+1];
    time_t	utsec;

    if ( (utsec = isodtime_to_unixtime( vobj, id )) == -1 )
	utsec = time( NULL );
    strftime( hms, sizeof(hms), "%H%M%S", localtime( &utsec ) );
    return hms;
}
