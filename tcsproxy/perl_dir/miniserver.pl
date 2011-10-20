use TACCutils;
use clib;
use LWP;
use LWP::MediaTypes;

sub DistillerInit {
    %gFiles = ();
    $gRoot = &Options_Find("proxy.home") or die "No proxy.home option found";
    $gPrefix = &Options_Find("frontend.aggString")
        or die "No frontend.aggString option found";

    # check that the webServerName option matches the prefix.  If not, workers
    # that rely on webServerName for fetching "support" documents may not work.

    my $wsname = &Options_Find("frontend.webServerName") || "[no entry found]";

    # match with or without trailing slash
    unless ($gPrefix =~ m!^$wsname/?!io) {
        &message("**WARNING** frontend.webServerName '$wsname' doesn't match\n".
                 "frontend.aggString '$gPrefix'...is this what you meant?\n");
    }

    # set up the mapping of file suffixes to MIME types...this should really be
    # configurable externally, in the gm_options file or something.

    %gMimeTypes = (&read_mime_types("$gRoot/mime.types"))
        or die "No file $gRoot/mime.types";
    
    %gCache = ();

    $gRoot .= "/wwwroot";
    &message("Root of WWW filesystem is $gRoot\n");

    $SIG{'HUP'} = \&flush_cache;
}

sub DistillerMain {
    my ($data,$hdrs,$type,%args) = @_;
    my ($filename,$file);
    
    # verify the prefix matches.  We should never get here unless this is the
    # case, but it's easy to double check.
    
    my $url = &get_my_url($hdrs);

    if ($url =~ m!^http://!i) {
        unless ($url =~ s/^$gPrefix//io) {
            &message("Error 404 (not on this server) on $url\n");
            return(&make_error(404, "<b>URL $url doesn't live on this server.</b>"));
        }
    }
    # if URL DOESN'T start with http://hostname, this part has already been
    # removed by the frontend hook, and only the pathname is left.
    # remove leading slash from remainder of URL, if any.
    $url =~ s!^/!!;

    # to find the file:
    # - try the URL appended to the root of the filesystem.
    # - if that fails, but the URL corresponds to a subdirectory name (or is
    #   empty), try to get index.html from that subdirectory.
    # - otherwise error.


    if ($url eq '') {
        $filename = "$gRoot/index.html";
    } elsif (-f "$gRoot/$url") {
        $filename = "$gRoot/$url";
    } elsif (-d "$gRoot/$url" && -e "$gRoot/$url/index.html") {
        $filename = "$gRoot/$url/index.html";
    } else {
        &message("Error 404 (not found) on $url\n");
        return(&make_error(404, "<b>URL '$url' not found on this server.</b>"));
    }

    # got the filename, see if we've cached it.  canonicalize slashes first...
    $filename =~ s!/+!/!g;
    my $cached = "";
    unless ( &fresh($filename) ) {
        $cached = "*";
        unless (&fetch_file($filename)) {
            &message("500 Fetch failed on $url\n");
            return (&make_error(500, "<b>Internal server error fetching '$url'</b>"));
        }
    }
    $file = $gCache{$filename};
    &message(sprintf("$url ==> [%s]%s $filename\n", $file->{type}, $cached));

    # special case for prefs form
    if ($file->{type} eq 'text/html') {
        &message("Forwarding text/html using X-Route\n");
        my $h = $file->{headers};
        $h =~ s!\r\n!\r\nX-Route: transend/text/html\r\n!;
        return($file->{data}, $h, $file->{type}, 10);
    } else {
        return($file->{data}, $file->{headers}, $file->{type}, 0);
    }
}

sub make_error {
    my ($errcode,$msg) = @_;
    $msg = &htmlify($msg) if $msg;
    my $h = new TACCutils::Headers("HTTP/1.0 $errcode", "");
    $h->header("Content-type", "text/html");
    $h->header("Content-length", length($msg));
    return($msg, $h->as_string, "text/html", 0);
}

sub fresh {
    my $filename = shift;
    exists $gCache{$filename}
    && (stat($filename))[9] <= $gCache{$filename}->{modtime};
}

sub fetch_file {
    my $filename = shift;
    my $hdrs = new HTTP::Response;
    my $type;
    
    unless (open(F,$filename)) {
        &message( "Error: $filename was readable before but now is not!" );
        delete $gCache{$filename};
        return undef;
    }

    $gCache{$filename}->{data} = join('', (<F>));
    $gCache{$filename}->{modtime} = (stat(F))[9];
    close F;
    
    if ( $filename =~ /\.([^.]+)$/ ) {
        $type = $gMimeTypes{lc $1} || &guess_media_type($filename);
    }
    $gCache{$filename}->{type} = $type;
    $gCache{$filename}->{headers} = join("\r\n",
                                         "HTTP/1.0 200 OK",
                                         "Content-type: " . $type,
                                         "Content-length: " .
                                         length($gCache{$filename}->{data}),
                                         "\r\n");
    1;
}
                 
sub message {
    &MonitorClient_Send("Tiny httpd messages", shift @_, "Log");
}

sub read_mime_types {
    my $fn = shift;
    open(MIME,$fn) or die "Cannot read $fn: $!";
    my $type;
    my %mime;
    while (<MIME>) {
        next if /^#/;
        chomp;
        split;
        next unless $#_ > 0;
        $type = shift;
        for (@_) { $mime{lc $_} = $type; }
    }
    close MIME;
    return %mime;
}

sub flush_cache {
    &message("Flushing cache\n");
    %gCache = ();
}

        
1;
