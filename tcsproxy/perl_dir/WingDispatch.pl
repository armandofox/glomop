use TACCutils;
use clib;

sub DistillerInit {
    my $status = &InitializeDistillerCache;
    return $status if ($status != 1);

    ## The list of MIME types we can safely pass back to the client, along
    ## with the first client version that supports them
    %exitok = ( 'application/x-wingman' => [0x01000100, 0],
                'application/x-palmosdb' => [0x01000300, 1],
                'application/x-wingman-agglist' => [0x01000200, 2] );
    
    ## The list of wingman distillers, keyed by MIME type
    %dispatch = ( 'text/plain' => 'wingman.text',
                  'text/html' => 'wingman.text',
		  'image/gif' => 'wingman.text',
		  'image/jpg' => 'wingman.text',
		  'image/jpeg' => 'wingman.text',
		  'application/zip' => 'wingman.zip',
		  'application/x-zip-compressed' => 'wingman.zip',
		  'application/x-wingman-agg' => 'wingman.agg' );

    $FRONT_CLI_VERSION = "i14";
    $FRONT_SRCFLAG = "i16";

    &message("Dispatcher running\n");
    0;
}

sub message {
    &MonitorClient_Send("WingDispatch messages", shift @_, "Log");
}

sub DistillerMain {
    my ($data,$hdrs,$type,%args) = @_;
    my $retcode;
    my @argarray;
    my $a;
    my ($at, $an);
    my $url = &get_my_url($hdrs);
    
    &message("Request for $url\n");
    foreach $a (keys %args) {
        ($at, $an) = $a =~ /^(.)(.*)$/;
	push(@argarray, &make_argument($at, $an, $args{$a}));
    }

    ## Figure out if we know something we can do
    while(1) {
	$type = lc $type;
	my $exok = $exitok{$type};
	if ($exok && $args{$FRONT_CLI_VERSION} >= $exok->[0]) {
	    $retcode = 0;
	    $type = "x-wingman/" . $exok->[1];
	    my $nhdrs = &make_headers($hdrs);
	    $nhdrs->content_type($type);
	    $hdrs = $nhdrs->as_string;
	    last;
        }
	$retcode = 0, last unless defined $dispatch{$type};
	my $nextone = $dispatch{$type};
	if ($args{$FRONT_SRCFLAG} == 2 && $type =~ m!^text/!) {
	    ## Make things a little nicer
	    $data =~ s/\0//gos;
	    $data =~ s/\r\n/\n/gos;
	    $data =~ s/\r/\n/gos;
	    $nextone = 'wingman.doc';
	}
	&message("Type is $type\n");
	($retcode, $data, $hdrs) = &Distill($type, $nextone, $hdrs,
					    $data, @argarray);
	my $nhdrs = &make_headers($hdrs);
	$type = $nhdrs->content_type;
	&message("Type is now $type\n");
	last if $retcode != 0;

	last if $nhdrs->status =~ /^HTTP\/\S+\s+3/; ## Drop out on redirect
    }

    &message("Output type is $type\n");
    return ($data,$hdrs,$type,$retcode);
}

1;
