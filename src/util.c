/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*/
	static char rcsid[] =
	"$Id: util.c,v 1.9 2001/06/11 09:14:59 tsch Rel $";		/*
*									*
* IC35 utilities: logging, errors, messages				*
*									*
*************************************************************************
*									*
*	conditional compile on NO_LOGSIM: if #defined, logging of the	*
*	IC35 communications is NOT supported, default WITH logging.	*
*	conditional compile on __STRICT_ANSI__: if #defined, substitute	*
*	functions, which are not available with the ANSI C standard.	*
*	(compilation with 'gcc -ansi ..' does #define __STRICT_ANSI__)	*
*									*
*		logging #ifndef NO_LOGSIM				*
*	log_init	initialize logfile, logtag and loglevel 	*
*	log_close	close logfile					*
*	lprintf		printf to logfile				*
*	ldump		dump data to logfile				*
*	log_proginfo	log program name, version and build info	*
*	log_argsinfo	log command line of program invocation		*
*		(error-)messages					*
*	vlmessage	local: vprintf message				*
*	message		output and log L_MESG message			*
*	error		output and log L_ERROR message			*
*	fatal		output,log L_FATAL message and abort program	*
*	_not_impl	report unimplemented feature			*
*		substitute functions #ifdef __STRICT_ANSI__		*
*	strncasecmp							*
*	strcasecmp							*
*	strdup								*
*									*
************************************************************************/

#include <stdio.h>	/* fprintf(), ..	*/
#include <stdarg.h>	/* va_start(), ..	*/
#include <stdlib.h>	/* malloc(), free(), ..	*/
#include <string.h>	/* strcpy(), .. 	*/
#include <sys/types.h>	/* size_t, ..		*/
#include <sys/time.h>	/* gettimeofday(), ..	*/
#include <time.h>	/* struct tm, time() .. */

#include "util.h"
NOTUSED(rcsid)


/* logging
*  -------
*	format of log entries:
*	hh:mm:ss.mmmm tag level message
*	hh:mm:ss.mmmm tag level  addr hex... char...
*/
#ifndef NO_LOGSIM
static char *	logfname = NULL;
static FILE *	logfp    = NULL;
static char *	logtag   = "";
static int	loglevel = L_DEBUG;

static int
_log_init( char * l_fname, char * l_tag, int level )
{
    if ( l_fname && *l_fname ) {
	logfp = fopen( logfname = l_fname, "w" );
	if ( logfp == NULL )
	    return ERR;
    }
    if ( l_tag && *l_tag )
	logtag = l_tag;
    if ( level )
	loglevel = level;
    return OK;
}
void
log_init( char * l_fname, char * l_tag, int level )
{
    if ( _log_init( l_fname, l_tag, level ) != OK )
	fatal( "cannot open logfile: %s", l_fname );
}
void
log_close( void )
{
    if ( logfp ) {
	fclose( logfp );
	logfp = NULL;
    }
}
static char *
ltstamp( void )
{
    static char 	timestamp[16];
    struct timeval	tv;
    struct tm *		ptm;

    gettimeofday( &tv, NULL );
    ptm = localtime( (time_t*)&tv.tv_sec );
    sprintf( timestamp, "%02d:%02d:%02d.%04d",
			ptm->tm_hour, ptm->tm_min, ptm->tm_sec,
			(int)(tv.tv_usec/100) );
    return timestamp;
}
static void
vltprintf( int level, char * tstamp, const char * format, va_list argp )
{
    if ( logfp == NULL || level > loglevel )
	return;
    fprintf( logfp, "%s %s %d ", tstamp, logtag, level );
    vfprintf( logfp, format, argp );
    fprintf( logfp, "\n" );
}
static void
vlprintf( int level, const char * format, va_list argp )
{
    vltprintf( level, ltstamp(), format, argp );
}
void
lprintf( int level, const char * format, ... )
{
    va_list	argp;

    va_start( argp, format );
    vlprintf( level, format, argp );
    va_end( argp );
}
static void
ltprintf( int level, char * tstamp, const char * format, ... )
{
    va_list	argp;

    va_start( argp, format );
    vltprintf( level, tstamp, format, argp );
    va_end( argp );
}
void
ldump( int level, void * data, size_t dlen, const char * format, ... )
{
    va_list	argp;
    char *	tstamp;
    int 	i;
    uchar	*dptr, *dend;
    char	*hdptr, hdump[5+3*16+1];
    char	*cdptr, cdump[16+1];

    tstamp = ltstamp();
    va_start( argp, format );
    vltprintf( level, tstamp, format, argp );
    va_end( argp );
    dend = (dptr = (uchar*)data) + dlen;
    while ( dptr < dend ) {
	hdptr = hdump;
	cdptr = memset( cdump, 0, sizeof(cdump) );
	sprintf( hdptr, "%04lX:", dptr - (uchar*)data ); hdptr += 5;
	for ( i = 0; i < 16 && dptr < dend; ++i, ++dptr ) {
	    sprintf( hdptr, " %02X", *dptr ); hdptr += 3;
	    *cdptr++ = ' ' < *dptr && *dptr <= '~' ? *dptr : '.';
	}
	ltprintf( level, tstamp, " %-53s %s", hdump, cdump );
    }
}

/* log program info
*  ----------------
*	current date and time, as log has only hh:mm:ss.mmmm stamps
*	program name, version and release date
*	build date+time, user, host
*/
void
log_proginfo( char * name, char * version, char * date, char * bldinfo )
{
    time_t	tnow;
    struct tm *	ptm;
    char	dtime[24+1];	/* yyyy-mm-dd hh:mm:ss zzzz */

    tnow = time( NULL );
    ptm = localtime( &tnow );
    strftime( dtime, sizeof(dtime), "%Y-%m-%d %T %Z", ptm );
    lprintf( L_MESG, "%s", dtime );
    lprintf( L_MESG, "%s %s (%s)", name, version, date );
    lprintf( L_MESG, "%s", bldinfo );
}

/* log command line
*  ----------------
*/
void
log_argsinfo( int argc, char ** argv )
{
    int 	i, len;
    char *	cmdline;

    for ( i = len = 0; i < argc; ++i )
	len += strlen( argv[i] ) + 1;
    if ( (cmdline = malloc( len )) != NULL ) {
	strcpy( cmdline, argv[0] );
	for ( i = 1; i < argc; ++i )
	    strcat( strcat( cmdline, " " ), argv[i] );
	lprintf( L_MESG, "cmd: %s", cmdline );
	free( cmdline );
    }
}
#endif /*NO_LOGSIM*/


/* output (error-)message
*  ----------------------
*/
static void
vlmessage( int level, const char * format, va_list argp )
{
    vfprintf( stderr, format, argp );
    fprintf( stderr, "\n" );
#ifndef NO_LOGSIM
    vlprintf( level, format, argp );
#endif
}
void
message( const char * format, ... )
{
    va_list	argp;

    va_start( argp, format );
    vlmessage( L_MESG, format, argp );
    va_end( argp );
}
void
error( const char * format, ... )
{
    va_list	argp;

    va_start( argp, format );
    vlmessage( L_ERROR, format, argp );
    va_end( argp );
}
void
fatal( const char * format, ... )
{
    va_list	argp;

    va_start( argp, format );
    vlmessage( L_FATAL, format, argp );
    va_end( argp );
    exit( 1 );
}

/* report unimplemented feature
*  ----------------------------
*/
int
_not_impl( char * feature )
{
    error( "%s NOT IMPLEMENTED", feature );
    return ERR;
}


/* substitute functions
*  --------------------
*	the functions below are unavailable on compilation with "-ansi",
*	which #defines __STRICT_ANSI__
*/
#ifdef	__STRICT_ANSI__
#include <ctype.h>	/* toupper()	*/

int
strncasecmp( const char * s1, const char * s2, size_t n )
{
    int 	cmp;

    for ( ; n; ++s1, ++s2, --n ) {
	cmp = toupper(*s1) - toupper(*s2);
	if ( cmp != 0 )
	    return cmp;
	if ( !( *s1 && *s2 ) )
	    break;
    }
    return 0;
}
int
strcasecmp( const char * s1, const char * s2 )
{
    size_t	n;

    if ( (n = strlen(s1)) < strlen(s2) )
	n = strlen(s2);
    return strncasecmp( s1, s2, n );
}

char *
strdup( const char * s )
{
    char *	nstr;

    if ( (nstr = malloc( strlen(s) + 1 )) != NULL )
	strcpy( nstr, s );
    return nstr;
}
#endif /*__STRICT_ANSI__*/
