/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*
*	$Id: util.h,v 1.9 2001/03/02 02:08:32 tsch Rel $  	*
*									*
* header for IC35 utilities						*
*									*
************************************************************************/
#ifndef _UTIL_H
#define	_UTIL_H 	1

#include <sys/types.h>	/* size_t	*/

#ifdef	HAVE_CONFIG_H
#include <config.h>	/* NO_LOGSIM	*/
#endif


#define NOTUSED(x)	static void _nu_ ## x( void ) \
			  { (void)( _nu_ ## x + 0 ); (void)( x + 0 ); }
#define UNUSED(x)	(void)( x + 0 )	/* LINT: unused function arg */

#if !defined(OK) || !defined(ERR)
# define OK	0
# define ERR	-1
#endif
#if !defined(TRUE) || !defined(FALSE)
# define TRUE	1
# define FALSE	0
#endif

#define bool	int
#define uchar	unsigned char
#define ushort	unsigned short
#define uint	unsigned int
#define ulong	unsigned long

#if !defined(min) || !defined(max)
# define min(a,b)		( (a) < (b) ? (a) : (b) )
# define max(a,b)		( (a) < (b) ? (b) : (a) )
#endif
#ifndef offsetof
# define offsetof(type,fld)	(size_t)&( ((type*)0)->fld )
#endif
#ifndef alenof
# define alenof(array)		( sizeof(array) / sizeof(array[0]) )
#endif


/* log levels	*/
#define L_FATAL 0	/* program will end,	   SYSLOG LOG_ALERT  */
#define L_ERROR 1	/* action should be taken, SYSLOG LOG_ERR    */
#define L_AUDIT 2	/* serious info,	   SYSLOG LOG_NOTICE */
#define L_WARN  3	/* something goes wrong ?	*/
#define L_MESG  4	/* info - level 1		*/
#define L_INFO  5	/* info - level 2		*/
#define L_NOISE 6	/* info - level 3		*/
#define L_DEBUG 7	/* debugging, very noisy	*/

#ifdef	NO_LOGSIM	/* log and com-simulation disabled */
#define LOG_INIT(args)
#define LOG_CLOSE(args)
#define LPRINTF(args)
#define LDUMP(args)
#define LOG_PROGINFO(args)
#define LOG_ARGSINFO(args)
#else /*!NO_LOGSIM*/	/* log and com-simulation enabled  */
#define LOG_INIT(args)		log_init args
#define LOG_CLOSE(args)		log_close args
#define LPRINTF(args)		lprintf args
#define LDUMP(args)		ldump args
#define LOG_PROGINFO(args)	log_proginfo args
#define LOG_ARGSINFO(args)	log_argsinfo args
#endif /*NO_LOGSIM*/


void log_init( char * l_fname, char * l_tag, int level );
void log_close( void );
void lprintf( int level, const char * format, ... );
void ldump( int level, void * data, size_t dlen, const char * format, ... );
void log_proginfo( char * name, char * version, char * date, char * bldinfo );
void log_argsinfo( int argc, char ** argv );

void message( const char * format, ... );
void error( const char * format, ... );
void fatal( const char * format, ... );

int _not_impl( char * feature );

#ifdef	__STRICT_ANSI__
int	strcasecmp( const char * s1, const char * s2 );
int	strncasecmp( const char * s1, const char * s2, size_t n );
char *	strdup( const char * s );
#endif /*__STRICT_ANSI__*/

#endif /*_UTIL_H*/
