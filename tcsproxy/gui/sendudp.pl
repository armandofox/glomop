#! /usr/sww/bin/perl

sub main {
    warn ("Usage: $0 [udp_port], default port is 9550\n"), exit 1
        if grep(/^-h/,@ARGV);
    warn "Type string contents of UDP pkt, \\n will be removed.  ^C to quit.\n";
    $z = connectsock gm_sock "full-sail",9550, "udp";
    while (1) {
        print STDERR "=> ";
        chomp($y = <STDIN>);
        $z->writesock($y);
    }
}

###########################################################################
#  gm_sock.pl: socket/stream abstractions
#  $Id: sendudp.pl,v 1.2 1996/11/22 01:29:11 yatin Exp $
#
#  Accept client connection on TCP/UDP socket, open a connection to a
#  TCP/UDP server, or create a "fake" socket from an already open (pair of)
#  filehandles.
#
#  Object methods:  All return nonzero for success.  In case of error,
#  call the lasterror() method to get error message text.
#
#  Constructors:
#
#  listensock(port, [proto, queuesize]): create socket object for listening
#       (server).  Proto is either 'tcp' or 'udp' (default 'tcp').
#  connectsock(host, port, [proto]): create socket object and connect
#       to host:port using proto (default 'tcp')
#  makesock(readfh, writefh): bless a pair of open filehandles into a socket
#        object; useful if you've already opened the socket yourself, or if you
#        want to use the read/write abstractions on plain old files
#
#  Methods: all return nonzero for success, zero for failure, unless otherwise
#       noted.  Call lasterror() to get a string description of failure cause.
#
#  acceptsock(): accept connection on created socket object; returns a new
#               socket object.  Ignored for udp sockets.
#  readsock([count]):  read & return a line of up to count bytes from accepted
#     connection; strips newlines.  For udp sockets, count param is required,
#     and if called in array context,a list containing the sender's IP address
#     (printable ASCII form) and the accepted dgram is returned.
#  readsock_raw([count]): read a line, don't strip anything; same comments
#     apply as for readsock.  For UDP sockets, readsock and readsock_raw are
#     equivalent.
#  writesock(list): write list to accepted connection, verbatim
#  selectsock(blocktimeout,sock,cond[,sock,cond....]): select for condition
#       'cond' on each of the socket objects.  'cond' is any subset
#       of the letters "rwe" (read,write,exception on socket).   'sock' is
#       either a socket object or a raw filehandle.  'blocktimeout' is the
#       length of time in millisecs to block before timing out: 0 means
#       nonblocking, -1 means block forever.  Returns undef if timeout,
#       otherwise returns list (object, cond, object, cond, ...) where 
#       each object is a socket object and cond is one or more letters
#       indicating which condition is ready for that preceeding socket
#       object.
#  closesock(): close socket object
#  killsock():  shutdown and close (shouldn't usually need to do this, since it
#               shuts down the original socket)
#  redirect(readfh,writefh):  Redirect this socket object to use these
#       filehandles for reading and writing (default: STDIN, STDOUT)
#  lasterror(): return string describing last error condition.  This can be
#       called as a subroutine or as a method.  The former is useful if an
#       error is returned by a constructor, e.g.:
#               unless ($sock = connectsock gm_sock $host,$port) {
#                     print STDERR &gm_sock::lasterror();
#               } 
#  eof(): return nonzero if eof has been seen on this socket during a read.
#
#  Methods that help you violate encapsulation:
#
#  filehandles(): returns read and write filehandles
#  endpoints(): (talkhost, talkport, listenhost, listenport) as strings; hosts
#       are given as IP addresses, ports as integers
#
###########################################################################

package gm_sock;
use Socket;

$FH = "SR0";

#
#  Determine name and address of local host
#

use POSIX 'uname';
$Hostname = (&uname)[1]
    or die "Cannot get hostname: $!";

$Myaddr = (gethostbyname($Hostname))[4];
die "Cannot get IP address of $Hostname: $!"
    unless $Myaddr;

###########################################################################
#  redirect socket to STDIN/STDOUT
#
###########################################################################

sub redirect {
    my $self = shift;
    my $fh = $self->{ReadFH};
    my($read,$write) = @_ ? @_ : ("STDIN", "STDOUT");
    my($result) = 1;
    
    open($write, ">&$fh") or $result = 0;
    open($read, "<&$fh") or $result = 0;

    select($write); $| = 1;
    select($read); $| = 1;

    return $result;
}

######################################################################
#  readsock: Read a "line" from already-open socket (up to CRLF) and remove
#  trailing CRLF
#  readsock_raw: don't strip trailing CRLF
######################################################################

sub readsock {
    my $self = shift;
    my $count = shift;
    my $addr;

    if ($self->{Protoname} eq 'tcp') {
        $_ = $self->readsock_raw();
        return $_ unless defined $_;
        chop while /[\r\n\012]$/;
        return $_;
    } else {
        ($addr, $_) = $self->readsock_raw($count);
        return (wantarray? () : undef) unless defined $_;
        chop while /[\r\n\012]$/;
        return (wantarray? ($addr, $_) : $_);
    }
}

sub readsock_raw {
    my $self = shift;
    my $count = shift || 0;
    my $sock = $self->{ReadFH};
    my $result = '';
    
    unless ($sock) {
        $self->{Error} = "No read filehandle";
        return undef;
    }
    
    if ($self->{Protoname} eq 'tcp') {
        select($sock); $| = 1; select(STDERR);
        $_ = <$sock>;

        $self->{EOF} = 1, $_ = ""
            unless defined $_;
    
        ($self->closesock()), $_ = ""
            unless $_;
        $result = $_;
    } else {
        ($self->{Error} = "Datagram receive requires a byte count",
         return undef)
            unless $count > 0;
        # connectionless: just do a receive
        my $addr = recv $sock,$result,$count,0;
        $self->{Error} = "recvfrom failed: $!" unless defined $addr;
        return ( wantarray ?
                (join('.',unpack('C4', (unpack('S n a4 x8',$addr))[2])), 
                 $result) :
                $result);
    }
}

sub eof
{
    my $self= shift;
    return ($self->{EOF});
}

######################################################################
#  writesock
#
#  Write a 'line' to already-open socket, appending a trailing newline.
######################################################################

sub writesock {
    my $self = shift;
    my $sock = $self->{WriteFH};
    unless ($sock) {
        $self->{Error} = "No write filehandle";
        return 0;
    }
    if ($self->{Protoname} eq 'tcp') {
        print $sock (@_);
    } else {
        $self->{Error} = "send failed: $!", return undef
            unless (send $sock,join(//, @_),0);
    }
    return 1;
}

###########################################################################
#  filehandles: return filehandle names for reading & writing
###########################################################################

sub filehandles {
    my $self = shift;

    my $readfh = $self->{ReadFH};
    my $writefh = $self->{WriteFH};

    return ('gm_sock::' . $readfh, 'gm_sock::' . $writefh);
}

###########################################################################
#  endpoints: return socket endpoints as list:
#  talkhost,talkport,listenhost,listenport 
###########################################################################

sub endpoints {
    my $self = shift;

    return ($self->{TalkHost}, $self->{TalkPort},
            $self->{ListenHost}, $self->{ListenPort});
}

###########################################################################
#  selectsock: implement select() on socket objs or fh's
###########################################################################

sub selectsock {
    my $timeout;
    my $cond;
    my $self;
    my ($nfound,$timeleft);
    my %fhs;
    
    $self = shift;
    $timeout = shift;
    my( @fhlist ) = @_;
    my @newfhlist;
    my ($rbits, $ebits, $wbits) = ('','','');

    while ($fh = shift @fhlist) {
	$cond = shift @fhlist;
	if (! ($cond =~ /r|w|e/)) {
            if ($self) {
		print STDERR "Bad conditions list $cond for socket\n";
                $self->{Error} = "Bad condition list '$cond' for socket";
            } else {
                $Err = "Bad condition list '$cond' for socket";
		print STDERR "Bad conditions list $cond for socket\n";
            }
            return (wantarray? () : undef);
        }
        push (@newfhlist, $fh = ($_->filehandles)[0]);
        $fhs{$fh} = $_;
        vec($rbits, fileno($fh->{ReadFH}), 1) = 1 if $cond =~ /r/i;
        vec($wbits, fileno($fh->{WriteFH}), 1) = 1 if $cond =~ /w/i;
        vec($ebits, fileno($fh->{ReadFH}), 1) = 1 if $cond =~ /e/i;
    }

    $timeout = undef if $timeout == -1;
    ($nfound,$timeleft) = select($rout = $rbits, $wout = $wbits, $eout =
                                 $ebits, $timeout);

    # if a socket was ready, return the socket object along with a string for
    # any conditions that are ready

    return undef unless $nfound > 0;

    $cond = '';
    undef @retA;
    foreach $fh (@newfhlist) {
	$fhcopy = $fh;
	($cond .= 'r') if (vec($rout, fileno($fh->{ReadFH}), 1));
	($cond .= 'w') if (vec($wout, fileno($fh->{WriteFH}), 1));
	($cond .= 'e') if (vec($eout, fileno($fh->{ReadFH}), 1));
	if ($cond) {
	    push @retA, ($fhs{$fhcopy}, $cond);
	}
    }
    return @retA;
}
        
    

###########################################################################
#  connectsock: Open socket connection to host:port
###########################################################################

sub connectsock {
    shift;
    my $self = &new();
    my($host, $port, $protoname) = @_;
    $protoname ||= 'tcp';
    $protoname = lc $protoname;
    $protomodel = ($protoname eq 'tcp'? SOCK_STREAM : SOCK_DGRAM);
    my($fh) = $FH++;
    my($name, $aliases, $proto) = getprotobyname($protoname);
    my($addr) = (gethostbyname($host))[4];
    my($me) = pack('S n a4 x8', AF_INET, 0, $Myaddr);
    my($them) = pack('S n a4 x8', AF_INET, $port, $addr);

    unless (socket($fh, PF_INET, $protomodel, $proto)) {
        $Err = "connectsock: socket: $!";
        return undef;
    }
    
    setsockopt($fh, SOL_SOCKET, SO_REUSEADDR, 1)
        || warn("Warning: setsockopt: $!");

    setsockopt($fh, SOL_SOCKET, SO_LINGER, pack("ii",1, 1000));

    unless (bind($fh, $me)) {
        $Err = "connectsock: bind: $!";
        return undef;
    }

    unless (connect($fh, $them)) {
        $Err = "connectsock: connect $host:$port: $!";
        return undef;
    }
    select($fh); $| = 1; select(STDOUT);

    $self->{ReadFH} = $self->{WriteFH} = $fh;
    $self->{TalkHost} = $Hostname;
    $self->{TalkPort} = "??";
    $self->{ListenHost} = $host;
    $self->{ListenPort} = $port;
    $self->{Protoname} = $protoname;
    return $self;
}

######################################################################
#  listensock: Open socket for listening for new client connections
#
#  Arguments: port, queue size
######################################################################

sub listensock {
    my $self = shift;
    my($port, $protoname, $qsize) = @_;
    my $readfh = $FH++;

    $qsize = 10 unless $qsize;
    $protoname = 'tcp' unless $protoname =~ /udp/i;
    $protoname = lc $protoname;
    my $protomodel = ($protoname eq 'tcp'? SOCK_STREAM : SOCK_DGRAM);

    local($name,$aliases,$proto) = getprotobyname($protoname);
    local($bind);

    # Check args

    unless ($port) {
        die "listensock: usage: listensock port [qsize]";
    }

    $bind = pack('S n a4 x8', AF_INET, $port, "\0\0\0\0");

    # create socket

    unless (socket($readfh, PF_INET, $protomodel, $proto)) {
        die( "listensock: socket($readfh): $!" );
        return 0;
    }
    setsockopt($readfh, SOL_SOCKET, SO_REUSEADDR, 1);

    # Bind socket to our addr/port num

    unless (bind($readfh, $bind)) {
        $Err =  "listensock: bind: $!";
        return 0;
    }

    if ($protoname eq 'tcp') {
        # listen for connections

        unless (listen($readfh, $qsize)) {
            $Err = "listensock: listen: $!";
            return 0;
        }
    }

    select($readfh); $| = 1; select(STDOUT);

    $self = &new();
    $self->{ReadFH} = $readfh;
    $self->{WriteFH} = $readfh;
    $self->{ListenHost} = join(".", unpack('C4', $Myaddr));
    $self->{ListenPort} = $port;
    $self->{Protoname} = $protoname;
    
    return $self;
}

###########################################################################
#  new: return a generic, blessed, anonymous hash for the socket object's
#  instance variables.  This is a private method.
###########################################################################

sub new
{
    return (bless {
        EOF => 0,
        ReadFH => "",
        WriteFH => "",
        Error => "",
        ListenHost => "",
        ListenPort => "",
        TalkHost => "",
        TalkPort => ""
        });
}
        

###########################################################################
#  makesock: take an already open read and write filehandle and treat them as a
#  socket object reference
###########################################################################

sub makesock
{
    my $self = shift;
    my($readfh,$writefh) = @_;

    select($readfh); $| = 1; select($writefh); $| = 1;
    
    $self = &new();
    $self->{ReadFH} = $readfh;
    $self->{WriteFH} = $writefh;
    return $self;
}

###########################################################################
#  lasterror: return the last error message generated by an operation on this
#  socket object.
###########################################################################

sub lasterror
{
    my $self = shift;
    unless (ref($self) ne 'gm_sock') {
        return $self->{Error};
    } else {
        return $Err;
    }
}

sub acceptsock
{
    my $self = shift;
    my ($af,$port,$inetaddr);
    my $newreadfh = $FH++;
    my $newwritefh = $newreadfh;
    my $addr;

    # if this is  a datagram socket, just return this instance.
    return $self if $self->{Protoname} ne 'tcp';

    my($addr) = accept($newreadfh, $self->{ReadFH});
    unless ($addr) {
        $self->{Error} = "Accept failed: $!";
        return undef;
    }

    ($af,$port,$inetaddr) = unpack('S n a4 x8', $addr);

    $selfcopy = &new();

    $selfcopy->{ListenPort} = $self->{ListenPort};
    $selfcopy->{ListenHost} = $self->{ListenHost};
    $selfcopy->{TalkHost} = join(".", unpack('C4', $inetaddr));
    $selfcopy->{TalkPort} = $port;
    $selfcopy->{ReadFH} = $newreadfh;
    $selfcopy->{WriteFH} = $newwritefh;
    $selfcopy->{Protoname} = $self->{Protoname};

    return $selfcopy;
}

sub closesock
{
    my $self = shift;
    close $self->{ReadFH};
    close $self->{WriteFH};
}

sub killsock
{
    my $self = shift;
    #shutdown($Sockfh, 2);
    close($self->{ReadFH});
}

###########################################################################
#  close_conn: close logical connection to client
###########################################################################

sub close_conn {
    my $self = shift;
    # any "bye" acknowledge should go here.
    closesock $self;
    return 1;
}

&main;
