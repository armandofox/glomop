#!/usr/sww/bin/perl -w
use Socket;

# poll every 30 sec; if no reply after 30 sec, save all core files,
# tcp_redirect.errs, then send mail to transend-bugs@latte

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
$admin = 'transend-bugs@latte.cs.berkeley.edu'; # for "normal" mail
$urgent = 'transend-bugs@latte.cs.berkeley.edu'; # for urgent mail
#$admin = 'gribble@latte.cs.berkeley.edu'; # for "normal" mail
#$urgent = 'gribble@latte.cs.berkeley.edu'; # for urgent mail
$runcmd = ("/usr/sww/bin/gmake run_tcp_redirect");
$sendmail = undef;                          # path to sendmail prog
$sin =undef;                                # sock addr to ping

$just_started = 1;                          # to suppress sending mail the
                                            # first time we check
$generation = 1;                            # successive saves

# where to look for sendmail, etc.
@bin = qw(/usr/sbin /usr/bin /usr/lib /usr/etc /usr/ucb);

$state = 1;

sub main {
    &init($optionfilename);
    my ($res,$where);
    while (1) {
        if ($state == 0) {
            # 
            # tcp_redirect just crashed
            # 
            $res = &restart_fe;
            sleep($timeout);
            if (($e = &ping_error($timeout)) eq '') {
                $state = 1;
            } else {
                &send_mail($urgent, "URGENT crash",
                           "tcp_redirect restart failed ($res$e), still trying");
                $state = 2;
            }
        } elsif ($state == 1) {
            #
            # system running OK
            #
            sleep($timeout) while (($e = &ping_error($timeout)) eq '');
            # ping failed!
            unless ($just_started) {
                $where = &save_state($dirname);
                &send_mail($admin, "TR-checker report",
                           "tcp_redirect crashed ($e), restarting $where");
            }
            $just_started = 0;
            $state = 0;
        } else {                                # state==2
            #
            #  restart failed
            #
            &restart_fe,sleep($timeout)
                until (&ping_error($timeout) eq '');
            &send_mail($urgent, "URGENT resolved", "tcp_redirect back up");
            $state = 1;
        }
    }
}

sub ping_error {
    my $to = shift;
    my($r,$w,$e) = ('','','');
    my($nf,$tl);
    my $reply = '';

    socket(SOCK, PF_INET, SOCK_STREAM, getprotobyname('tcp')) or die $!;
    bind(SOCK, sockaddr_in(0,inet_aton("localhost"))) or die $!;
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
        return '';
    } else {
        return "Request got zero bytes from FE";
    }
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

sub save_state {
    my $dir = shift;
    my $newdir = "$dir/save.$$.$generation";
    $generation++;
    
    mkdir($newdir, 0777)
        or return "(Couldn't create state dir $newdir: $!)";
    if ($res=system("/bin/cp */core tcp_redirect/tcp_redirect.errs $newdir")) {
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
        $sendmail = "$_/sendmail" if -x "$_/sendmail";
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

    my $port = ($opt{'tcp_redirect.listen_port'}
                || die "No port number in options file?");

    # generates global $sin and filehandle SOCK
    $host = shift @ARGV || "localhost";
    $sin = sockaddr_in($port,inet_aton($host)) or die $!;
}

sub send_mail {
    # assumes: global $sendmail is path to executable sendmail
    my ($to,$subject,$msg) = @_;
    return unless $to;
    my $date = "" . localtime;
    my $text =
        "From: tcp_redirect-checker\nTo: $to\nDate: $date\nSubject: $subject\n"
            . "-----\n$msg\n";
    
    if (open(MAIL, "| $sendmail -t")) {
        print MAIL $text;
        close MAIL;
    } else {
        warn "Mail would be: $text";
    }
}

warn "Checker pid is $$\n";
&main;
