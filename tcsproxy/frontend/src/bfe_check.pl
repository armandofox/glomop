#!/usr/sww/bin/perl
use Socket;
use IO::Socket;

# poll every 30 sec; if no reply after 30 sec, save all core files,
# frontend/frontend.errs, then send mail to transend-bugs@latte

# here is the pseudocode for the state machine

# State 0:
#   restart FE
#   if (successful ping before timeout) {
#     goto state 1
#   } else {
#     send email "restart failed, will keep trying"
#     goto state 2
#   }
# State 1: (system successfully running)
#   while (successful ping) {
#     stay in this state
#   }
#   send email "frontend crashed, trying to restart"
#   goto state 0
# State 2: (previous restart failed)
#   while (! (successful ping)) {
#     stay in this state
#   }
#   send email "frontend back up"
#   goto state 1

# command line args

my $optionfilename = shift @ARGV;
die "Reading options file: $!" unless -r $optionfilename;

my $dirname = ".";                 # where to put subdirs that save corefiles
die "Can't create save dirs in $dirname!"
    unless -d $dirname && -w $dirname;

$timeout = pop @ARGV;
$timeout = 30 if $timeout<30;
$runcmd = ("/usr/sww/bin/gmake run_frontend");
$sendmail = undef;                          # path to sendmail prog
$http_sin =undef;                                # sock addr to ping

$just_started = 1;                          # to suppress sending mail the
                                            # first time we check
$generation = 1;                            # successive saves

# where to look for sendmail, etc.
@bin = qw(/usr/sbin /usr/bin /usr/lib /usr/etc /usr/ucb);

$state = 1;
&init($optionfilename);

if (($admin eq '') || ($urgent eq '')) {
   print STDERR "\n";
   print STDERR "Error: frontend.admin_email/urgent_email undefined in options file $optionfilename";
   print STDERR "\n";
   exit(0);
}


sub main {
    my ($res,$where,$kres);
    my $STATE_OK=1,$STATE_WING_BAD=2,$STATE_HTTP_BAD=3,$STATE_RESTART_BAD=4,
       $STATE_CRASHED=0;
    my ($ewing,$ehttp);
    $state = $STATE_OK;
    while (1) {
        if ($state == $STATE_CRASHED) {
            # 
            # frontend just crashed
            # 
            # kill it if in memory..
            $kres = &kill_fe;
            if ($kres != 0) {
                &send_mail($urgent, "URGENT crash",
              "Frontend restart failed ( Could not kill frontend ), still trying");
            } else {
                $res = &restart_fe;
                sleep($timeout);
                if ((($ehttp = &ping_http($timeout,$http_sin)) eq '') &&
                    (($ewing = &ping_wing($timeout,$wingport,$host)) eq '')) {
                    $state = $STATE_OK;
                } else {
                    &send_mail($urgent, "URGENT crash",
                           "Frontend restart failed ($res $ewing $ehttp), still trying");
                
                    $state = $STATE_RESTART_BAD;
               }
            }
        } elsif ($state == $STATE_OK) {
            #
            # system running OK
            #
	    my $ok = 1;
            while ($ok) {
                $ehttp = &ping_http($timeout,$http_sin);
                $ewing = &ping_wing($timeout,$wingport,$host);
                if (($ewing ne '') || ($eping ne '')) {
                   $state = $STATE_CRASHED;
                   # save core files, etc..
                   $where = &save_state($dirname);
                   my $errmsg = '';
                   $errmsg .= "Wingman aspect down: $ewing " if ($ewing ne '');
                   $errmsg .= "HTTP aspect down: $ehttp " if ($ehttp ne '');
                   &send_mail($admin, "FE-checker report",
                             "Frontend crashed: ($errmsg), restarting $where");
                   $ok = 0;
                } else {
                    sleep($timeout)
                }
            }
        } else {                                # state==2
            #
            #  restart failed
            #
            &restart_fe,sleep($timeout)
                until ((&ping_http($timeout,$http_sin) eq '') && 
                       (&ping_wing($timeout,$wingport,$host) eq ''));
            &send_mail($urgent, "URGENT resolved", "Frontend back up");
            $state = 1;
        }
    }
}

sub ping_http {
    my $to = shift;
    my $sin = shift;
    my($r,$w,$e) = ('','','');
    my($nf,$tl);
    my $reply = '';

    socket(SOCK, PF_INET, SOCK_STREAM, getprotobyname('tcp')) or die $!;
    bind(SOCK, sockaddr_in(0,inet_aton("127.0.0.1"))) or die $!;
    return "Connect: $!" unless connect(SOCK, $sin);
    vec($r,fileno(SOCK),1) = 1;
    vec($e,fileno(SOCK),1) = 1;
    
    select SOCK; $| = 1; select STDOUT;
    print SOCK "GET foobarfoobar HTTP/1.0\r\n\r\n";

    while (1) {
        ($nf,$tl) = select($ro=$r,$w,$eo=$e,$to);
        if ($nf == 0) {                     # timeout
            close SOCK;
            return "Timeout waiting for proxy reply";
        }
        if ($nf == -1) {                    # i/o error?
            close SOCK;
            return "select() I/O error: $!";
        }
        if (vec($eo,fileno(SOCK),1)) {       # exception
            close SOCK;
            return "Filehandle exception: $!";
        }
        if (vec($ro,fileno(SOCK),1)) {       # readable, yay
            last;
        }
    }
    
    $reply .= $_ while ($_ = <SOCK>);
    close SOCK;
    if (length($reply) > 0) {
         if ($reply =~ /x-proxy-error/i) {
            return "Could not start HTML distiller.";
         } else { 
            return '';
         }
    } else {
        return "Request got zero bytes from FE";
    }
}

sub ping_wing {
    my $to = shift;
    my $port = shift;
    my $addr = shift;
    my($r,$w,$e) = ('','','');
    my($nf,$tl,$sock);

    eval('$sock = new IO::Socket::INET(PeerAddr => $addr, PeerPort => $port, Proto => \'tcp\') || die "Could not create socket";');
    return "Error: $!" if ($! ne '');

    $out = pack("H8H8H8H8H8H8H8H8H8H8H8H8", 
                               "01050200","00000001","00000004","00000018",
                               "00000001","00040000","68747470","3a2f2f77",
                               "77772e63","65726b65","6c65792e","65647500");
    $sock->write($out,length($out));
    $sock->flush;
    
    my $buf = read_all($sock, 30);
    $sock->close;
    return "Request got zero bytes from FE" if (length($buf) == 0);
    return "";
}
            

sub restart_fe {
    my $result;
    $result   = system($runcmd);
    if ($result) {
        return "exit $result -->";
    } else {
        return '';
    }
}

# A hack for now. Will get more legitmate information later..
sub kill_fe {
    my $count = shift;
    my $result,@lines,$found=0,$i=0;
   if ($count == 2) {
      return -1;
   }
   $result = `/usr/ucb/ps -auxww | egrep sbin/frontend | egrep transend`;
   @lines = split(/\n/,$result);
   while ((!$found) && ($i<@lines)) {
      if ($lines[$i] !~ /egrep/) {
         $found = 1;
      } else { $i++; }
   }
   if ($found) {
      ($c,$pid) = split(/ +/,$lines[$i]);
      if ($count == 1) {
          `kill -9 $pid`;
      } else {
         `kill $pid`; 
         sleep(4);
         `kill $pid`; 
          sleep(4);
          kill_fe($count+1);
      }
   } else {
      return 0;
   }
}

sub save_state {
    my $dir = shift;
    my $newdir = "$dir/save.$$.$generation";
    $generation++;
    
    mkdir($newdir, 0777)
        or return "(Couldn't create state dir $newdir: $!)";
    if ($res=system("/bin/mv core */core frontend.errs frontend/frontend.errs $newdir")) {
        return "(Couldn't save state in dir $newdir: exit $res)";
    } else {
        return "(Saved state in $newdir)";
    }
}

sub init {
    my $optionfilename = shift;
    my %opt;

    undef $sendmail;
    for (@bin) {
        $sendmail = "$_/sendmail.orig" if -x "$_/sendmail.orig";
    }
    $" = ' ', die "Can't find sendmail in @bin"  #"
        unless defined $sendmail;

    open(OPT, $optionfilename) or die $!;
    while (<OPT>) {
        chop;
        next if /^!/;
        next if /^\s*$/;
        $opt{$1} = $2 if /^\s*([^:]+):\s*(.*)$/;
    }
    close OPT;

    my $httpport = ($opt{'frontend.http_listen_port'}
                || die "No port number in options file?");
    $wingport = ($opt{'frontend.wing_listen_port'}
                || die "No port number in options file?");
    $admin = ($opt{'frontend.admin_email'} 
                || die "No frontend.admin_email defined in options file?");
    $urgent = ($opt{'frontend.urgent_email'} 
                || die "No frontend.urgent_email defined in options file?");

    # generates global $sin and filehandle SOCK
    $host = shift @ARGV || "127.0.0.1";
    $http_sin = sockaddr_in($httpport,inet_aton($host)) or die $!;
}

sub send_mail {
    # assumes: global $sendmail is path to executable sendmail
    my ($to,$subject,$msg) = @_;
    return unless $to;
    my $date = "" . localtime;
    my $text =
        "From: Frontend-checker\nTo: $to\nDate: $date\nSubject: $subject\n"
            . "-----\n$msg\n";
    
    if (open(MAIL, "| $sendmail -t")) {
        print MAIL $text;
        close MAIL;
    } else {
        warn "Mail would be: $text";
    }
}

sub read_all {
    my ($sock, $size) = @_;

    my $read = 0;
    my ($buf, $totbuf);
    while ($read < $size) {
        $bytes = $sock->read($buf, $size - $read);
        if ($bytes == 0) {
        return "";
        }
        $read += $bytes;
        $totbuf .= $buf;
    }

    return $totbuf;
}


warn "Checker pid is $$\n";
&main;

