#!/bin/sh
#
#	$Id: ic35log.sh,v 1.6 2001/01/14 00:09:46 tsch Rel $
#
# process logs of IC35 communication
#

# setup parameters and defaults
PAGER="less -S"
PROGNAME=`basename $0`
TMPDIR=/tmp/dpkgmirr$$

usage()
{
	echo "\
usage:  $PROGNAME [options] [tarfile] logfile
process IC35 communication 'logfile', which may be gzip'd or contained
in optionally gzip'd 'tarfile'. options:
  -l level	level of output:
     all	  show all communication (default)
     syn2	  suppress Level-1 of IC35sync protocol
     syn4	  decode Level-4 of IC35sync protocol
  -s n		prepend output with 1:linenumber 2:timestamp
  -a		show communication also as ASCII characters
  -D		debug/detail: output diagnostic data \
"
	exit 2
}

# add ASCII chars to hex_logfile
#
showascii()	# <hex_logfile >hex_ascii_logfile
{
    awk '
BEGIN {
			# create table chr[] for hex to char conversion
	ascii = ascii " !\"#$%&\047()*+,-./"
	ascii = ascii "0123456789:;<=>?"
	ascii = ascii "@ABCDEFGHIJKLMNO"
	ascii = ascii "PQRSTUVWXYZ[\\]^_"
	ascii = ascii "`abcdefghijklmno"
	ascii = ascii "pqrstuvwxyz{|}~"
	for ( c = 0; c <= 255; ++c )
	    if ( 32 <= c && c <= 126 )	# 32=space 126=tilda
		chr[ sprintf("%02X", c) ] = substr( ascii, c - 32+1, 1 )
	    else
		chr[ sprintf("%02X", c) ] = "."
}
/WRx/ || /RDx/ {
	if ( $1 == "WRx" || $1 == "RDx" ) {
	    if ( $1 == "WRx" )  dir = "WRa"
	    else		dir = "RDa"
	    beghex = 3
	} else {
	    if ( $2 == "WRx" )  dir = "WRa"
	    else		dir = "RDa"
	    beghex = 4
	}
	ascblk = chr[$(beghex)]
	for ( i = beghex+1; i <= NF; ++i )
	    ascblk = ascblk "  " chr[$i]
	print
	if ( beghex == 3 )
	    printf( "%s %2d\t %s\n", dir, $2, ascblk )
	else
	    printf( "%s %s %2d\t %s\n", $1, dir, $3, ascblk )
}' -
}

# convert 'portmon' logfile
#	expects 'portmon' logfiles in Unix-format i.e. NL terminated lines
#	(DOS-format CRLF terminated lines may break the awk script)
#	for 'portmon' see http://www.sysinternals.com
#
ic35prot()	# [-vskip=3] [-vDEBUG=1] <portmon_logfile >hex_logfile
{
    awk $* '
#   parameters
#	skip	number of leading bytes to skip from transmitted blocks
#		blocks up to this much bytes are not printed, default 0
#	stamp	if non-zero/non-empty stamp transmit blocks with id (first
#		field) from portmon log, to ease finding portmon log lines
#		default: 0 / "" (disabled)
#	MAXBPL	maximum number of bytes to print per output line
#		default: 128
#	DEBUG	if non-zero/non-empty print verbose debugging output
#		default: 0 / "" (disabled)

# extract transmitted data bytes
#    returns:
#	-1	no data bytes: miss "Length" field
#	0	no data bytes: length value == 0
#	> 0	length value
#	xdlen	number of logged bytes, maybe < length value
#	xdata	logged transmitted data bytes
#
function getxdata()
{
	xlen = 0
	xdata = ""
			# search "Length" field
	for ( i = 1; i <= NF; ++i )
	    if ( $i ~ "^Length" )
		break
	if ( i > NF ) {		# "Length" field not found
				# check if log from "ic35sync"
	    for ( i = 1; i <= NF; ++i )
		if ( $i ~ "^[0-9A-F][0-9A-F][0-9A-F][0-9A-F]:$" )
		    break;
	    if ( i > NF )
		return -1
				# process log from "ic35sync"
	    xdata = ""
	    for ( i = i+1; i <= NF; ++i ) {
		if ( $i !~ "^[0-9A-F][0-9A-F]$" )
		    break;
		xdata = xdata $i " "
		if ( --hadxmit == 0 )
		    break;
	    }
	    xdlen = length( xdata ) / 3
	    return xdlen
	}
			# get length value from next field, remove trail ":"
	split( $(i+1), flds, ":" )
	len = int( flds[1] )
	if ( len == 0 )
	    return 0		# no data transmitted
			# extract get data bytes after "Length" and <nn:>
	lentxt = $(i) " " $(i+1)
	xdata = substr( $0, index($0, lentxt) + length(lentxt)+1 )
			# make string of 2dig-hex with trail " " for append
	sub( "^[^0-9A-Fa-f]*", "", xdata );	# remove leading garbage
	sub( "[^0-9A-Fa-f]*$", "", xdata );	# remove trailing garbage
	xdata = xdata " "
	xdlen = length( xdata ) / 3
			# return length value, maybe not all bytes logged
	return len
}

function prxdata( dir, xbuf, xlen,
		    _offs, _i, _hex, _pbuf, _plen )
{
	if ( xlen <= skip )
	    return
	_plen = length( xbuf )
	_offs = skip * 3
	if ( skip >= 3 )
	    _plen = length( xbuf ) - _offs - 2 * 3	# skip checksum also
	printf( "%s%sx %2d\t%s\n", xstamp, dir, xlen, substr(xbuf,_offs,_plen) )
}

BEGIN {
			# max.number of bytes per output line
	if ( ! MAXBPL ) MAXBPL = 128
}

NF >= 2 {
	if ( $2 == "ic35sync" || $2 == "ic35mgr" )
	    tsecs = $1
	else
	    tsecs += $2
}
# Windows-98:
# 35  0.00034960  Ic35 syn  VCOMM_ReadComm  COM1  SUCCESS  Length: 1: 57
/VCOMM_ReadComm/ && /SUCCESS/ {
	if ( oldxdir == "WR" ) do_print = 1
	xdir = "RD"
	if ( DEBUG ) print "DBG: xdir=" xdir, $0
}
# 36  0.00005040  Ic35 syn  VCOMM_GetCommQueueStatus  COM1  SUCCESS  RX: 6 TX: 0
/VCOMM_GetCommQueueStatus/ {
	if ( DEBUG ) print "DBG: next   ", $0
	next
}
# 58  0.00063840  Ic35 syn  VCOMM_WriteComm  COM1  SUCCESS  Length: 1: 41
/VCOMM_WriteComm/ {
	if ( oldxdir == "RD" ) do_print = 1
	xdir = "WR"
	if ( DEBUG ) print "DBG: xdir=" xdir, $0
}

# Windows-NT:
# 22      0.00106488      IC35Mgr.exe     IRP_MJ_READ     Serial1 SUCCESS Length 1: 80
# 16  0.00000000  IC35 Sync.exe  IRP_MJ_READ  Serial0  Length 1
# 16  0.00000953  SUCCESS  Length 1: 57
/IRP_MJ_READ/ {
	if ( oldxdir == "WR" ) do_print = 1
	if ( $0 ~ "SUCCESS" ) {
	    xdir = "RD"
	    if ( DEBUG ) print "DBG: xdir=" xdir, $0
	} else if ( $0 !~ "TIMEOUT" ) {
	    hadread = 1
	    if ( DEBUG ) print "DBG: hadread", $0
	} else {
	    do_print = 1
	}
}
# 16  0.00000953  SUCCESS  Length 1: 57
/SUCCESS/ && hadread {
	xdir = "RD"
	hadread = 0
	if ( DEBUG ) print "DBG: dlyd RD", $0
}
# 15  0.07746540  TIMEOUT  Length 0:
/TIMEOUT/ && hadread {
	do_print = 1
}
# 419  0.00000000  IC35 Sync.exe  IOCTL_SERIAL_CLR_DTR  Serial0
/IOCTL_SERIAL_CLR_DTR/ {
	do_print = 1
}
# 3  0.00000000  IC35 Sync.exe  IRP_MJ_WRITE  Serial0  Length 1: 41
/IRP_MJ_WRITE/ {
	if ( oldxdir == "RD" ) do_print = 1
	hadread = 0
	xdir = "WR"
	if ( DEBUG ) print "DBG: xdir=" xdir, $0
}

# ic35sync/Linux
# 01:11:48.3206 ic35sync 6 com_send(0x8055180,19) = 19
# 01:11:48.3206 ic35sync 6  0000: 02 13 00 83 0E 00 49 0B 00 01 07 03 00 00 00 00
# 01:11:48.3206 ic35sync 6  0010: 00 05 01
/ com_send\(/ {
	if ( oldxdir == "RD" ) do_print = 1
	hadxmit = $NF
	xmitdir = "WR"
}
# 01:11:48.3703 ic35sync 6 com_recv(0xbffff89b,1) = 1
# 01:11:48.3703 ic35sync 6  0000: F2
# 01:11:48.3706 ic35sync 6 com_recv(0x8055181,2) = 2
# 01:11:48.3706 ic35sync 6  0000: 10 00
# 01:11:48.3800 ic35sync 6 com_recv(0x8055183,13) = 13
# 01:11:48.3800 ic35sync 6  0000: A0 0B 00 49 08 00 0A 00 00 06 20 2E 02
/ com_recv\(/ {
	if ( oldxdir == "WR" ) do_print = 1
	hadxmit = $NF
	xmitdir = "RD"
}
/  [0-9A-F][0-9A-F][0-9A-F][0-9A-F]: / && hadxmit {
	xdir = xmitdir
}

DEBUG {
	print "DBG1: old,xdir=" oldxdir "," xdir, "xbuff=" xblen, "\047" xbuff "\047"
}
do_print {
	prxdata( oldxdir, xbuff, xblen )
	if ( txlen > xblen ) {
	    txdir = (oldxdir == "WR" ? "Wx " : "Rx ")
	    print txdir txlen
	}
	xbuff = ""
	xblen = txlen = 0
	do_print = 0
}
xdir {
	if ( DEBUG ) print "DBG: do_x=" xdir, $0
	xlen = getxdata()		# get transmit data to xdata,xdlen
	if ( xlen <= 0 )
	    next			#  no transmit / timeout
	if ( xblen == 0 && stamp ) {
	    if ( stamp == 2 )
	      if ( tsecs ~ ":" )
		xstamp = tsecs "  "
	      else
		xstamp = sprintf( "%010.4f  ", tsecs )
	    else
		xstamp = sprintf( "%05d\t", $1 )
	}
	xbuff = xbuff xdata		# buffer transmit data
	xblen += xlen
	txlen += xlen
	if ( xdlen != xlen || xblen >= MAXBPL ) { # not all logged or too much
	    prxdata( xdir, xbuff, xblen )
	    xbuff = ""
	    xblen = 0
	}
	oldxdir = xdir
	xdir = ""
}
DEBUG {
	print "DBG2: old,xdir=" oldxdir "," xdir, "xbuff=" xblen, "\047" xbuff "\047"
}
' -
}

# decode IC35sync Level-4 protocol
#	expects output from ic35prot()
#
ic35L4prot()	# [-vDEBUG=1] <hex_logfile >L4prot_logfile
{
    awk $* '
# parameters
#	DEBUG	if non-zero/non-empty print verbose debugging output
#		default: 0 / "" (disabled)

BEGIN {
			# create table chr[] for hex to char conversion
	ascii = ascii " !\"#$%&\047()*+,-./"
	ascii = ascii "0123456789:;<=>?"
	ascii = ascii "@ABCDEFGHIJKLMNO"
	ascii = ascii "PQRSTUVWXYZ[\\]^_"
	ascii = ascii "`abcdefghijklmno"
	ascii = ascii "pqrstuvwxyz{|}~"
	for ( c = 0; c <= 255; ++c ) {
	    xc = sprintf("%02X", c)
	    if ( 32 <= c && c <= 126 )	# 32=space 126=tilda
		chr[ xc ] = substr( ascii, c - 32+1, 1 )
	    else
		chr[ xc ] = "\\x" xc
	}
}

# get block of bytes from hex transmit data
# convert to ASCII and enclose in " quotes
#
function ascblk( beg, len,  _end, _blk, _i )
{
	if ( (_end = beg + len-1) > NF )
	    _end = NF			# truncate to end of input line
	_blk = "\""
	for ( _i = beg; _i <= _end; ++_i )
	    _blk = _blk chr[ $_i ]
	_blk = _blk "\""
	return _blk
}

# get byte block of bytes from hex transmit data
#
function hexblk( beg, len,  _end, _blk, _i )
{
	if ( (_end = beg + len-1) > NF )
	    _end = NF			# truncate to end of input line
	_blk = $beg
	for ( _i = beg+1; _i <= _end; ++_i )
	    _blk = _blk " " $_i
	return _blk
}

# convert hex strings to decimal
#
function hex2dec( hexstr,  _xval, _xdig, _i )
{
    if ( hexstr ~ "^0[Xx]" )
	hexstr = substr(hexstr, 3)
    _xval = 0
    for ( _i = 1; _i <= length(hexstr); ++_i ) {
	_xdig = index( "0123456789ABCDEFabcdef", substr(hexstr, _i, 1) ) - 1
	if ( _xdig < 0 )
	    break
	if ( _xdig >= 16 )
	    _xdig -= 16 - 10
	_xval = _xval * 16 + _xdig
    }
    return _xval
}

# decode field data from IC35 record
function recdata( beg, fid,  _i, _recblk )
{
	if ( fid != "" ) {	# file-id specified: this the header fragment
	    nflens = 0
	    if      ( fid == "05" ) nflens = 21	# Addresses
	    else if ( fid == "06" ) nflens =  4	# Memo
	    else if ( fid == "07" ) nflens =  8	# ToDoList
	    else if ( fid == "08" ) nflens = 10	# Schedule
	    if ( nflens == 0 ) {
		printf( "ERR: recdata(beg=%s,fid=%s) unkown fid\n", beg, fid )
		return ""
	    }
	    delete flens
	    for ( _i = 1; _i <= nflens; ++_i )
		flens[_i] = hex2dec( $(beg+_i-1) )
	    fidx = 1
	    fldrest = 0
	}
	if ( fldrest ) {	# partial last field from previous fragment
	    _recblk = " f" fidx-1 "=" ascblk( beg, fldrest )
	    sub( "=\"", "=<", _recblk )
	    _i = beg + fldrest
	    fldrest = 0
	} else {		# record data on field boundary
	    _recblk = ""
	    if ( fid != "" )  _i = beg + nflens		# header fragment
	    else	      _i = beg			# 2nd ff. fragment
	}
	while ( _i <= NF && fidx <= nflens ) {
	    _recblk = _recblk " f" fidx "=" ascblk( _i, flens[fidx] )
	    _i += flens[fidx++]
	}
	if ( _i != NF+1 ) {	# partial last field
	    fldrest = _i - (NF+1)
	    sub( "\"$", ">", _recblk )
	}
	return _recblk
}

# WRx 43   80 26 00 10 00 64 00 4A 1F 00 49 4E 56 45 4E 54 45 43 ...
# WRx 14   83 09 00 49 06 00 02 00 00
# WRx  8   83 00 03
# RDx 46   A0 29 00 10 00 D0 07 4A 22 00 49 4E 56 45 4E 54 45 43 ...
# RDx 13   A0 08 00 49 05 00 03 01
# RDx  8   A0 03 00
# variables:
#	type	indicates command "Cmd" "C??" or response "Rsp" "R??"
#		"C??","R??" flag PDU contents not matching current knowledge
#	pi	parameter index: PDU paramters begin at pi-th input field
#	pdu	PDU identification for output, e.g. "identify", "openfile"
#	pars	decoded parameters from PDU for output
#	l2l3	concatenation of Level-2 and Level-3 id-bytes
#	l4	Level-4 command
#	l2l3l4	concatenation of l2l3 and l4
#	printhex  flag to also output the hex input (for debugging)
#		  may be set by some decoder, reset on every input block
#	DEBUG	  global debug flag to also output the hex input
#
/^WRx/ || /^RDx/ {
	if ( $1 == "WRx" ) {
	    type = "Cmd"
	    pi = 2+3+3+2 + 1			# command parameter index
	} else { # "RDx"
	    type = "Rsp"
	    pi = 2+3+3 + 1			# response parameter index
	}
	printhex = 0
	pdu = pars = l2l3 = l4 = l2l3l4 = ""
	if        ( NF >= 2+3+3 ) {		# have L2- and L3 id-bytes
	    l2l3 = $3 $6
	    if    ( NF >= 2+3+3+2 ) {		# have also L4 command
		l4   = $9 $10
		l2l3l4 = l2l3 l4
	    }
# identify
	    if        ( l2l3   == "8010" ) {
#   AdsGetDeviceVersion ?
		pdu = "identify"
	    } else if ( l2l3   == "A010" ) {
		pdu = "identify"
		pars = ascblk(pi+25, 99)		# "DCS15" version

# power, passwd
	    } else if ( l2l3l4 == "8249" "0301" ) {
		pdu = "power"
		pars =          ascblk(pi, 5)		# "Power"
		pars = pars " " hexblk(pi+5, 99)	# 00 00 00
		if ( pars != "\"Power\" 00 00 00" )
		    type = "C??"
	    } else if ( l2l3   == "A049" && lastcmd == "power" ) {
		pdu = "power"
		pars = $(pi) $(pi+1)			# 0301
		if ( pars != "0301" || NF+1 - pi != 2 )
		    type = "R??"
	    } else if ( l2l3l4 == "8249" "0300" ) {
		pdu = "passwd"
		pars = ascblk(pi, 8)			# [password]
		if ( NF+1 - pi != 8 )
		    type = "C??"
	    } else if ( l2l3   == "A049" && lastcmd == "passwd" ) {
		pdu = "passwd"
		pars = $(pi) $(pi+1)			# 0101
		if ( pars != "0101" || NF+1 - pi != 2 )
		    type = "R??"

# getdtime, setdtime
	    } else if ( l2l3l4 == "8349" "0200" ) {
#   AdsReadSysInfo ?
		pdu = "getdtime"
		pars = hexblk(pi, 99)			# 00
		if ( pars != "00" )
		    type = "C??"
	    } else if ( l2l3   == "A049" && lastcmd == "getdtime" ) {
		pdu = "dtime"
		pars = ascblk(pi, 14)			# [mmddyyyyhhmmss]
		tail = hexblk(pi+14, 99)		# 00 00
		if ( tail != "00 00" ) {
		    pars = pars " " tail
		    type = "R??"
		}
	    } else if ( l2l3l4 == "8249" "0201" ) {
#   AdsWriteSysInfo ?
		pdu = "setdtime"
		head = hexblk(pi, 1)			# 00
		pars = ascblk(pi+1, 14)			# [mmddyyyyhhmmss]
		tail = hexblk(pi+1+14, 99)		# 00 00
		if ( head != "00" && tail != "00 00" ) {
		    pars = head " " pars " " tail
		    type = "C??"
		}
	    # response		  "A0"
	    #   pdu = "done"	below

# category
	    } else if ( l2l3l4 == "8249" "0302" ) {
#   AdsReadCategoryData ?
		pdu = "category"
		pars = ascblk(pi, 8)			# [category]
		if ( NF+1 - pi != 8 )
		    type = "C??"
	    } else if ( l2l3   == "A049" && lastcmd == "category" ) {
		pdu = "category"
		pars = $(pi) $(pi+1)
		if ( NF+1 - pi != 2 )
		    type = "R??"

# openfile, closefile
	    } else if ( l2l3l4 == "8249" "0002" ) {
#   AdsOpenDataBase ?
		pdu = "openfile"
		head = hexblk( pi, 7 )
		lf2 = hex2dec( $(pi+10) $(pi+9) $(pi+8) $(pi+7) )
		lf  = hex2dec( $(pi+11) )
		fn  = ascblk(pi+12, lf)
		tail = hexblk( pi+12+lf, 99 )
		if ( length( fn ) == lf + 2	\
		  && lf2 == lf + 2 \
		  && head == "00 00 00 00 00 00 00" \
		  && tail == "02" )
		    pars = fn
		else {
		    pars = head " lf2=" lf2 " lf=" lf " " fn " " tail
		    type = "C??"
		}
	# set current file-id, to determine num.of record fields
		if      ( fn ~ "Addresses"  ) currfid = "05"
		else if ( fn ~ "Memo"       ) currfid = "06"
		else if ( fn ~ "To Do List" ) currfid = "07"
		else if ( fn ~ "Schedule"   ) currfid = "08"

	    } else if ( l2l3   == "A049" && lastcmd == "openfile" ) {
		pdu = "openfile"
		pars = "fd=" $(pi+1) $(pi)
		if ( NF+1 - pi != 2 )
		    type = "R??"
	    } else if ( l2l3l4 == "8249" "0003" ) {
#   AdsCloseDataBase ? AdsCloseHandle ?
		pdu = "closefile"
		pars = "fd=" $(pi+1) $(pi)
		tail = hexblk( pi+2, 99 )
		if ( tail != "00 00 00 00" ) {
		    pars = pars " " tail
		    type = "C??"
		}
	# unset current file-id on close
		currfid = ""
	    # response		  "A0"
	    #   pdu = "done"	below

# getflen
	    } else if ( l2l3l4 == "8349" "0103" ) {
#   AdsGetRecordCount
		pdu = "getflen"
		pars = "fd=" $(pi+1) $(pi)
		tail = hexblk( pi+2, 99 )
		if ( tail != "00 00 00 00" ) {
		    pars = pars " " tail
		    type = "C??"
		}
	    } else if ( l2l3l4 == "8349" "0104" ) {
#   AdsGetModifiedRecordCount
		pdu = "getflenmod"
		pars = "fd=" $(pi+1) $(pi)
		tail = hexblk( pi+2, 99 )
		if ( tail != "00 00 00 00" ) {
		    pars = pars " " tail
		    type = "C??"
		}
	    } else if ( l2l3   == "A049" && (lastcmd == "getflen" \
					  || lastcmd == "getflenmod") ) {
		pdu = lastcmd
		pars = "n=" hex2dec( $(pi+1) ($pi) )
		if ( NF+1 - pi != 2 )
		    type = "R??"

# read record
	    } else if ( l2l3l4 == "8349" "0105" ) {
#   AdsReadRecordByID
		pdu = "readrecrid"
		pars =       "fd=" $(pi+1) $(pi)
		pars = pars " ri=" $(pi+3) $(pi+2)
		pars = pars " "    $(pi+4)
		pars = pars " fi=" $(pi+5)
		reqrid = hex2dec( $(pi+5) $(pi+4) $(pi+3) $(pi+2) )
		if ( NF+1 - pi != 6 )
		    type = "C??"
	    } else if ( l2l3l4 == "8349" "0106" ) {
#   AdsReadRecordByIndex
		pdu = "readrec"
		reqrid = ""
		pars =       "fd=" $(pi+1) $(pi)
		pars = pars " ix=" hex2dec( $(pi+3) $(pi+2) )
		tail = hexblk( pi+4, 99 )
		if ( tail != "00 00" ) {
		    pars = pars " " tail
		    type = "C??"
		}
	    } else if ( l2l3l4 == "8349" "0107" ) {
#   AdsReadNextModifiedRecord
		pdu = "readrecmod"
		reqrid = ""
		pars =       "fd=" $(pi+1) $(pi)
		tail = hexblk(pi+2, 99)
		if ( NF+1 - pi != 6 || tail != "00 00 00 00" ) {
		    pars = pars " " tail
		    type = "C??"
		}
	    } else if ( (l2l3  == "A049" \
		     ||  l2l3  == "2048") && (lastcmd == "readrec" \
					   || lastcmd == "readrecmod" \
					   || lastcmd == "readrecrid") ) {
		pdu = "rechdr" (l2l3 == "2048" ? "(more)" : "(last)")
		rsprid = hex2dec( $(pi+3) $(pi+2) $(pi+1) $(pi) )
		pars =       "ri=" $(pi+1) $(pi)
		pars = pars " "    $(pi+2)
		pars = pars " fi=" $(pi+3)
		pars = pars " ch=" $(pi+4)
		if ( NF+1 - pi == 5 ) 
		    pars = pars " (nodata)"
		else if ( lastcmd == "readrecrid" && rsprid != reqrid )
		    pars = pars "  notfound (" NF+1 - (pi+5) " bytes garbage)"
		else
		    pars = pars " " recdata( pi+5, $(pi+3) )
		reqrid = ""
	    } else if ( (l2l3  == "A049" \
		     ||  l2l3  == "2048") && lastcmd == "readmore" ) {
		pdu = "recdat" (l2l3 == "2048" ? "(more)" : "(last)")
		pars = "                      "
		pars = pars " " recdata( pi, "" )

# write record
	    } else if ( l2l3l4 == "0248" "0108" \
	             || l2l3l4 == "8249" "0108" ) {
#   AdsWriteRecord
		pdu = "writerec" (l2l3 == "0248" ? "(more)" : "(last)")
		fdesc = $(pi+1) $(pi)
		zero1 = hexblk(pi+2, 4)
		p678  = hexblk(pi+6, 3)
		zero2 = hexblk(pi+9, 2)
		lrec  = hex2dec( $(pi+14) $(pi+13) $(pi+12) $(pi+11) )
		if ( zero1 == "00 00 00 00" && zero2 == "00 00" ) {
		    pars = "fd=" fdesc " " p678 " lr=" lrec
		    pars = pars " " recdata( pi+15, currfid )
		} else {
		    pars = "fd=" fdesc " " zero1 " " p678 " " zero2 " lr=" lrec
		    pars = pars " " recdata( pi+15, currfid )
		    type = "C??"
		}
# update record
	    } else if ( l2l3l4 == "0248" "0109" \
	             || l2l3l4 == "8249" "0109" ) {
#   AdsUpdateRecord
		pdu = "updatrec" (l2l3 == "0248" ? "(more)" : "(last)")
		pars1 =        "fd=" $(pi+1) $(pi)
		pars1 = pars1 " ri=" $(pi+3) $(pi+2)
		pars1 = pars1 " "    $(pi+4)
		pars1 = pars1 " fi=" $(pi+5)
		pars1 = pars1 " "    hexblk(pi+6, 3)
		zero2 = hexblk(pi+9, 2)
		pars2 =       " lr=" hex2dec( $(pi+14) $(pi+13) $(pi+12) $(pi+11) )
		if ( zero2 == "00 00" ) {
		    pars = pars1 pars2
		    pars = pars " " recdata( pi+15, currfid )
		} else {
		    pars = pars1 " " zero2 pars2
		    pars = pars " " recdata( pi+15, currfid )
		    type = "C??"
		}
	    } else if ( (l2l3  == "0248" \
	              || l2l3  == "8249") && (lastcmd == "writerec(more)" \
					   || lastcmd == "updatrec(more)") ) {
		pdu = substr(lastcmd, 1, 8)
		pdu = pdu (l2l3 == "0248" ? "(more)" : "(last)")
		pars = "                      "
		pars = pars " " recdata( pi-2, "" )
	    # response  l2     == "90"
	    #	pdu = "writemore"	below
	    } else if ( l2l3   == "A049" && (lastcmd == "writerec(last)" \
					  || lastcmd == "updatrec(last)") ) {
		pdu = substr(lastcmd, 1, 8)
		pars =       "ri=" $(pi+1) $(pi)
		pars = pars " "    $(pi+2)
		pars = pars " fi=" $(pi+3)
		if ( NF+1 - pi != 4 )
		    type = "R??"

# delete record
	    } else if ( l2l3l4 == "8249" "0102" ) {
#   AdsDeleteRecord
		pdu = "deleterec"
		pars =       "fd=" $(pi+1) $(pi)
		pars = pars " ri=" $(pi+3) $(pi+2)
		pars = pars " "    $(pi+4)
		pars = pars " fi=" $(pi+5)
		if ( NF+1 - pi != 6 || $(pi+4) != "00" )
		    type = "C??"
	    # response l2      == "A0"
	    #   pdu = "done"		below

# reset changeflag
	    } else if ( l2l3l4 == "8249" "010A" ) {
#   AdsCommitRecord
		pdu = "resetchg"
		pars =       "fd=" $(pi+1) $(pi)
		pars = pars " ri=" $(pi+3) $(pi+2)
		pars = pars " "    $(pi+4)
		pars = pars " fi=" $(pi+5)
		if ( $(pi+4) != "00" || NF+1 - pi != 6 )
		    type = "C??"
	    # response l2      == "A0"
	    #   pdu = "done"		below

	    }

# short PDUs with Level-2 id-byte only
	} else if ( NF == 2+3 ) {
	    if        ( $3 == "83" ) {
		pdu = "readmore"
	    } else if ( $3 == "90" ) {
		pdu = "writemore"
	    } else if ( $3 == "81" ) {
		pdu = "disconn"
	    } else if ( $3 == "A0" ) {
		pdu = "done"
	    }
	}

	if ( $1 == "WRx" ) {
	    lastcmd = pdu
	    if ( pdu == "" ) type = "C??"	# unknown command PDU
	    if ( type !~ "^C..$" )
		print "INTERNAL ERROR"
	} else { # "RDx"
	    lastrsp = pdu
	    if ( pdu == "" ) type = "R??"	# unknown response PDU
	    if ( type !~ "^R..$" )
		print "INTERNAL ERROR"
	}

	if ( type == "C??" || type == "R??" \
	  || DEBUG || printhex )
	    print			# show hex data for unknown PDU / params
	printf( "%s %2d\t", type, $2 )
	if ( pdu == "" ) {
	    for ( i = 3; i <= NF; ++i ) printf( " %s", $i )
	    printf( "\n" )
	} else { 
	    printf( " %-14s %s\n", pdu, pars )
	}
}
' -
}

# process options
#
LEVEL=all
while getopts "l:s:aDh" OPT
do	case $OPT in
	l )	LEVEL=$OPTARG			;;# level of output
	s )	STAMP="-vstamp=$OPTARG"		;;# prepend lineno/time stamp
	a )	ASCII=1				;;# also ASCII chars
	D )	DEBUG="-vDEBUG=1" ; KEEPTMP=1	;;# debug/diagnostic
	h|? )	usage			;;# for other options give help and exit
	esac
done
shift `expr $OPTIND - 1`
[ $# -eq 1 -o $# -eq 2 ] || usage

# the real work ..
#
case "$LEVEL" in
all)
    PASS1="ic35prot $STAMP $DEBUG"
    if [ -n "$ASCII" ]
    then  PASS2=showascii
    else  PASS2=cat
    fi
    ;;
syn2)
    PASS1="ic35prot -vskip=3 $DEBUG"
    if [ -n "$ASCII" ]
    then  PASS2=showascii
    else  PASS2=cat
    fi
    ;;
syn4)
    PASS1="ic35prot -vskip=3"
    PASS2="ic35L4prot $DEBUG"
    ;;
*)
    echo "$PROGNAME -- unsupported output level: $LEVEL"
    exit 1
esac

if [ $# -eq 2 ] ; then
    if [ "$1" != "-" ] && expr "`file $1`" : '.*\<gzip.*' >/dev/null ; then
	tar -xOzf $1 $2 | tr -d '\015' | $PASS1 | $PASS2 | $PAGER
    else
	tar -xOf  $1 $2 | tr -d '\015' | $PASS1 | $PASS2 | $PAGER
    fi
else
    if [ "$1" != "-" ] && expr "`file $1`" : '.*\<gzip.*' >/dev/null ; then
	zcat $1 | tr -d '\015' | $PASS1 | $PASS2 | $PAGER
    else
	cat  $1 | tr -d '\015' | $PASS1 | $PASS2 | $PAGER
    fi
fi
