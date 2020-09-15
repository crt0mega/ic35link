/************************************************************************
* Copyright (C) 2000 Thomas Schulz					*
*									*/
	static char rcsid[] =
	"$Id: comio.c,v 1.13 2001/03/03 16:30:45 tsch Rel $";		/*
*									*
* IC35 serial communication
*									*
*************************************************************************
*									*
*	conditional compile on NO_LOGSIM: if #defined, the simulated	*
*	communication is NOT supported, default WITH com-simulation.	*
*	conditional compile on __STRICT_ANSI__: if #defined, substitute	*
*	functions, which are not available with the ANSI C standard.	*
*	(compilation with 'gcc -ansi ..' does #define __STRICT_ANSI__)	*
*									*
*		simulated communication #ifndef NO_LOGSIM		*
*	com_siminit	initialize communications with simulation file	*
*	com_simexit	local: leave simulated communication		*
*	com_simul	local: report if simulation active		*
*	com_simrecv	local: simulate receive from simulation file	*
*		real communication
*	com_setsigs	local: set RS232 output signals			*
*	com_settimeout	set receive timeout, return previous		*
*	com_init	initialize serial communication device		*
*	com_waitnice	lower process priority when using com_sendw()	*
*	com_sendw	send datablock to comm.device with waiting	*
*	com_send	send datablock to comm.device			*
*	com_recv	receive datablock from comm.device		*
*	com_exit	close serial communication device		*
*									*
************************************************************************/

#include <stdio.h>	/* FILE*, fopen(), ..	*/
#include <string.h>	/* strncmp(), ..	*/
#include <unistd.h>	/* read(), write(), ..	*/
#include <termios.h>	/* tcgetattr(), ..	*/
#include <fcntl.h>	/* F_GETFL, ..		*/
#include <sys/ioctl.h>	/* ioctl(), ..		*/
#include <sys/types.h>	/* size_t, ..		*/
#include <sys/time.h>	/* struct timeval	*/
#include <signal.h>	/* sigaction(), ..	*/

#include "util.h"	/* LPRINTF(), ..	*/
#include "comio.h"
NOTUSED(rcsid)


/* ==================================== */
/*	simulated communication		*/
/* ==================================== */
/*
*	uses post-processed log of real communication with IC35:
*	  WR nn   xx xx xx ...
*	  RD nn   xx xx xx ...
*	and "receives" data using the "RD nn" lines.
*/
#ifndef NO_LOGSIM
static FILE *	simfp    = NULL;

static int
_com_siminit( char * s_fname )			/* init simulated comm	*/
{
    if ( s_fname && *s_fname ) {
	simfp = fopen( s_fname, "r" );
	if ( simfp == NULL )
	    return ERR;
    }
    return OK;
}
void
com_siminit( char * s_fname )
{
    if ( _com_siminit( s_fname ) != OK )
	fatal( "cannot open simulation file: %s", s_fname );
}
static void
com_simexit( void )				/* leave simulated comm	*/
{
    if ( simfp ) {
	fclose( simfp );
	simfp = NULL;
    }
}
static bool
com_simul( void )				/* report if simul active  */
{
    return (bool)( simfp != NULL );
}
static int
com_simrecv( uchar * buff, size_t blen )	/* receive from simul.file */
{
    static int	simrlen = 0;
    static int	simridx = 0;
    bool	do_check;
    uchar *	bptr;
    int		chr, n, rbyte;
    char	xdir[8];

    if ( buff == NULL || blen == 0 )		/* sanity */
	return 0;

    memset( buff, 0, blen);	/* clear buffer sets dummy bytes */
    bptr = buff; do_check = FALSE;
    while ( bptr < buff + blen ) {
			/* forward to or check next "RD nn" line */
	if ( simridx >= simrlen ) {
	    for ( ; ; ) {
		while ( (chr = fgetc( simfp )) != '\n' )
		    if ( chr < 0 )
			return ERR;
	    	if ( fscanf( simfp, "%s %d", xdir, &simrlen ) == 2
		  && strncmp( xdir, "RD", 2 ) == 0 )
		    break;
		if ( do_check )
		    return bptr - buff;
	    }
	    do_check = TRUE;
	    simridx = 0;
	}
			/* read bytes from "RD nn" line	*/
	for ( ; simridx < simrlen; ++simridx ) {
	    if ( bptr >= buff + blen )
		return blen;
	    ungetc( chr = fgetc( simfp ), simfp );  /* avoid fscanf() eat \n */
	    if ( chr == '\n'
	      || fscanf( simfp, "%x", &rbyte ) != 1 )
		break;
	    *bptr++ = (uchar)rbyte;
	}
			/* dummy non-logged recv bytes	*/
	n = min( blen - (bptr - buff), simrlen - simridx);
	simridx += n;
	bptr += n;
    }
    return bptr - buff;
}
#endif /*NO_LOGSIM*/


/* ==================================== */
/*	real communication		*/
/* ==================================== */

static int	com_fd = -1;
static int	com_tmo = 500;		/* timeout 500 ms	*/

/* local: set RS232 output signals
*  -------------------------------
*/
static void
com_setsigs( int sigs )
{
    int 	flags;

    if ( com_fd >= 0
      && ioctl( com_fd, TIOCMGET, &flags ) == 0 ) {
	flags &= ~(TIOCM_DTR|TIOCM_RTS);
	flags |=  (TIOCM_DTR|TIOCM_RTS) & sigs;
	ioctl( com_fd, TIOCMSET, &flags );
	LPRINTF(( L_NOISE, "com_setsigs(%08X) DTR %s RTS %s",
			    sigs, sigs & TIOCM_DTR ? "ON " : "off",
				  sigs & TIOCM_RTS ? "ON " : "off" ));
    }
}

/* local: log state of RS232 signals
*  ---------------------------------
*/
static void
_com_sigchg( void )
{
    static int	oflags = -1;
    static struct {
	char *	  name;
	int	  sig;
    }		sigtab[] = {
		    { "CTS", TIOCM_CTS	},
		    { "DCD", TIOCM_CAR	},
		    { "DSR", TIOCM_DSR	},
		    { "RI",  TIOCM_RNG	},
		    {  NULL,	0	}
		}, *psig;
    int 	flags;
    char	sigtext[48];

    if ( com_fd >= 0
      && ioctl( com_fd, TIOCMGET, &flags ) == 0
      && ( ((oflags ^ flags) & (TIOCM_CTS|TIOCM_CAR|TIOCM_DSR|TIOCM_RNG)) != 0
	  || oflags == -1 ) ) {
	strcpy( sigtext, "" );
	for ( psig = sigtab; psig->name; ++psig )
	    sprintf( sigtext+strlen(sigtext), " %s %s%s",
		    psig->name,
		    flags & psig->sig ? "ON" : "off",
		    ((oflags ^ flags) & psig->sig) && oflags != -1 ? "*" : "" );
	LPRINTF(( L_NOISE, "com signals:%s", sigtext ));
	oflags = flags;
    }
}

/* set,get receive timeout
*  -----------------------
*	return previous timeout. zero or negative timeout argument 'msec'
*	will not be set and can be used to enquire current timeout.
*/
int
com_settimeout( int msec )
{
    int 	old_tmo = com_tmo;

    old_tmo = com_tmo;
    if ( msec > 0 )
	com_tmo = msec;
    if ( old_tmo != com_tmo )
	LPRINTF(( L_NOISE, "com_settimeout %d -> %d", old_tmo, com_tmo ));
    return old_tmo;
}

/* initialize comm.device
*  ----------------------
*	open in O_NONBLOCK mode to avoid hanging on inactice DCD signal,
*	finally set back to blocking mode.
*	set line parameters to 115200,N,8,2 and raw mode, 2 stopbits are
*	needed to avoid IC35 receive errors with MMCard operations.
*	raise DTR and RTS signals.
*/
#ifdef	__STRICT_ANSI__
#define cfmakeraw(tiop) \
    (tiop)->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON); \
    (tiop)->c_oflag &= ~(OPOST);					\
    (tiop)->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);		\
    (tiop)->c_cflag &= ~(CSIZE|PARENB);					\
    (tiop)->c_cflag |= CS8;
#endif

int
com_init( char * devname )
{
    int 		i, flags;
    struct termios	tio;

#ifndef NO_LOGSIM
    if ( com_simul() ) {
	com_fd = -1;
	return OK;
    }
#endif
	/* open serial communication device	*/
    if ( (com_fd = open( devname,  O_RDWR|O_NONBLOCK|O_NOCTTY )) == -1 ) {
	return ERR;
    }
    if ( !isatty( com_fd ) ) {
	close( com_fd );
	com_fd = -1;
	return ERR;
    }
	/* set linepars 11500,N,8,2 and raw mode */
	/* HUPCL to clear DTR,RTS on last close  */
    tcgetattr( com_fd, &tio );
    tio.c_oflag = 0;
    tio.c_iflag = IGNBRK | IGNPAR;
    tio.c_cflag = CREAD | CLOCAL | CS8 | HUPCL | CSTOPB;
    cfsetispeed( &tio, B115200 );
    cfsetospeed( &tio, B115200 );
    tio.c_lflag = NOFLSH;
    cfmakeraw( &tio );
    for ( i = 0; i <= NCCS; ++i ) tio.c_cc[i] = 0;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;
    tcsetattr( com_fd, TCSANOW, &tio );
	/* set line signals DTR on, RTS off	*/
    com_setsigs( TIOCM_DTR|TIOCM_RTS );
	/* back to blocking mode for timeout	*/
    if ( (flags = fcntl( com_fd, F_GETFL, 0 )) != -1 ) {
	flags &= ~O_NONBLOCK;
	fcntl( com_fd, F_SETFL, flags );
    }
    _com_sigchg();

    return OK;
}

/* send datablock with waiting
*  ---------------------------
*	IC35 gets receive receive errors if datablocks of more than 16 bytes
*	are sent at full speed of 115200 baud, using 2 stopbits increases
*	this size limit to 29 bytes.
*	the problem occurred only with IC35 MMCard operations, import and
*	synchronize PIM-data don't suffer this limitation.
*	as a workaround do delay some time after sent every couple of bytes.
*	the delay is done with busy waiting using gettimeofday(), because
*	interrupt controlled delays are either too long (at least 10 msec
*	with an "itimer" at Linux-2.x timer interrupt frequency of 100 Hz)
*	or require root privilege (nanosleep() or fast RTC interrupts).
*	other methods like usleep(), nanosleep(), select() yielded delays
*	of ca. 20..30 msec and were therefore also not chosen.
*	(see getitimer(2), sigaction(2), nanosleep(2), usleep(3), select(2)
*	for reference).
*	to avoid too much CPU hogging during busywait delays, the process
*	scheduling priority may be lowered using nice(2) when comsendw()
*	is called with a block of more than NICE_MINBLEN bytes.
*	com_waitnice() is used to enable the priority lowering: each call
*	of com_waitnice() causes one nice() systemcall.
*	the maximum throughput measured with write file to IC35 MMCard was
*	ca. 1740 b/s with 2kB buffersize (with "itimer" it was ca. 1350 b/s,
*	with usleep() delay it was only ca. 600 b/s).
*	measurements on a Pentium-I/133MHz showed that
*	for blocks of   1     8    16    24    28 bytes
*	the delay of  165  1570  3180  4795  5620 usec was needed at least.
*	measurements on a Pentium-III/500MHz (by Harald Becker) showed that
*	for blocks of   1          16             bytes
*	the delay of  187        3212             usec was needed at least.
*/
#define NICE_MINBLEN	32	/* min. block length for nice() */
#define WAIT_BLKSIZE	16	/* number of bytes per write()	*/
#define WAIT_USDELAY	3250	/* usec to wait before write()	*/

static int	_waitnice_inc;	/* increment to use for nice(2) */

static void			/* busy wait some microseconds		*/
_wait_usec( long usec ) 	/* contributed by Harald Becker 	*/
{
    long		elapsed;	/* elapsed delay in usec	*/
    struct timeval	tbeg, tnow;	/* start time and current time	*/

    gettimeofday( &tbeg, NULL );	/* get start time of delay loop */
    do {				/* loop ..			*/
	gettimeofday( &tnow, NULL );		  /* get current time	*/
	if ( (elapsed = tnow.tv_sec - tbeg.tv_sec) )	/* seconds diff */
	   elapsed *= 1000000;			  /* to usec if nonzero	*/
	elapsed += tnow.tv_usec - tbeg.tv_usec;   /* add microsecs diff */
    } while( elapsed < usec );		/* .. until specified usec over */
}

void				/* export interface to store increment	*/
com_waitnice( int inc ) 	/* 'inc' for lowering process priority	*/
{
    _waitnice_inc = inc;
}

int				/* send data to communication device	*/
com_sendw( uchar * data, size_t dlen )		/* with busywait delays */
{
    uchar *	dptr;
    int 	slen;

#ifndef NO_LOGSIM
    if ( com_simul() )
	slen = dlen;
    else
#endif
    {
	if ( com_fd < 0 )
	    return ERR;
	LPRINTF(( L_DEBUG, "com_sendw(%p,%u) ..", data, dlen ));
	if ( dlen >= NICE_MINBLEN && _waitnice_inc != 0 ) {
	    nice( _waitnice_inc );	/* lower own process priority	*/
	    _waitnice_inc = 0;		/* no more nice() before next	*/
	}				/* com_waitnice() tells again	*/
	dptr = data;
	while ( (slen = data+dlen - dptr) > 0 ) {
	    if ( slen > WAIT_BLKSIZE ) slen = WAIT_BLKSIZE;
	    _wait_usec( WAIT_USDELAY );
	    if ( write( com_fd, dptr, slen ) != slen )
		break;
	    dptr += slen;
	}
	slen = dptr - data;
    }
    LDUMP(( L_NOISE, data, dlen,
		    "com_sendw(%p,%u) = %d", data, dlen, slen ));
    return slen;
}

/* send datablock to comm.device
*  -----------------------------
*/
int
com_send( uchar * data, size_t dlen )
{
    int 	slen;

#ifndef NO_LOGSIM
    if ( com_simul() )
	slen = dlen;
    else
#endif
    {
	if ( com_fd < 0 )
	    return ERR;
	LPRINTF(( L_DEBUG, "com_send(%p,%u) ..", data, dlen ));
	slen = write( com_fd, data, dlen );
    }
    LDUMP(( L_NOISE, data, dlen,
		    "com_send(%p,%u) = %d", data, dlen, slen ));
    return slen;
}

/* receive datablock from comm.device
*  ----------------------------------
*	use select() to wait for data, accumulate data with read()
*	until buffer 'buff' gets full or receive timeout occurs.
*/
int
com_recv( uchar * buff, size_t blen )
{
    fd_set		rfds;
    struct timeval	tmo;
    uchar		*rptr, *rend;
    int 		rlen;

#ifndef NO_LOGSIM
    if ( com_simul() )
	rlen = com_simrecv( buff, blen );
    else
#endif
    {
	if ( com_fd < 0 )
	    return ERR;
	rend = (rptr = buff) + blen;
	do {
	    FD_ZERO( &rfds );
	    FD_SET( com_fd, &rfds );
	    tmo.tv_sec  =  com_tmo / 1000;
	    tmo.tv_usec = (com_tmo % 1000) * 1000;
	    if ( select( com_fd+1, &rfds, NULL, NULL, &tmo ) > 0
	      && FD_ISSET( com_fd, &rfds ) ) {
		_com_sigchg();
		rlen = read( com_fd, rptr, rend - rptr );
		LPRINTF(( L_DEBUG, "com_recv: read(com,rptr,%d) = %d",
							rend-rptr, rlen ));
		if ( rlen > 0 )
		    rptr += rlen;
	    } else {
		rlen = 0;
	    }
	} while ( rlen > 0 && rptr < rend );
	rlen = rptr - buff;
    }
    if ( rlen > 0 )
	LDUMP(( L_NOISE, buff, rlen,
			  "com_recv(%p,%u) = %d", buff, blen, rlen ));
    else
	LPRINTF(( L_NOISE, "com_recv(%p,%u) = %d", buff, blen, rlen ));

    return rlen;
}

/* close comm.device
*  -----------------
*	leave simulated communication if active.
*	clear RS232 signals DTR and RTS and close comm.device.
*/
void
com_exit( void )
{
#ifndef NO_LOGSIM
    com_simexit();
#endif
    if ( com_fd < 0 )
	return;
    com_setsigs( 0 );				/* DTR off, RTS off */
    _com_sigchg();
    close( com_fd );
    com_fd = -1;
}

/* substitute for usleep()
*  -----------------------
*	the usleep() function is unavailable on compilation with "-ansi",
*	which #defines __STRICT_ANSI__
*/
#ifdef	__STRICT_ANSI__
void
usleep( unsigned long usec )
{
    struct timeval	tmo;

    tmo.tv_sec  = usec / 1000000;
    tmo.tv_usec = usec % 1000000;
    select( 0, NULL, NULL, NULL, &tmo );
}
#endif /*__STRICT_ANSI__*/
