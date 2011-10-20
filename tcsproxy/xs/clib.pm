package clib;

require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
        Options_Find
        MonitorClient_Send

        Clib_Fetch
	Clib_Redir_Fetch
	Clib_Async_Fetch
        Clib_Push
        Clib_Query
	Clib_Post

        InitializeDistillerCache
        Distill
	DistillAsync
	DistillRendezvous
	DistillDestroy
        make_argument     
);

=head1 NAME

clib - TACC server interface for Perl workers

=head1 SYNOPSIS

       use clib;

       &MonitorClient_Send($fieldname, $fieldvalue, $proc);

       ($status,$headers,$data) = &Clib_Fetch($url,$client_headers);
       $status = &Clib_Async_Fetch($url,$client_headers);
       ($status,$headers,$data) = &Clib_Post($url,$client_headers,
					      $data,$data_len);
       $status = &Clib_Push($url,$server_headers,$data);
       $status = &Clib_Query($url);

=head1 DESCRIPTION

=head2 Getting Configuration Options

=over 4

=item $val = &Options_Find($key)

Returns the string value of the option named by $key in the current
options file.  Currently, an empty string is returned if the option is
not defined, or is defined to have the empty string as its value.  (This
could be considered a bug; undefined options should really return
undef.  Tough beans.)

=back

=head2 Using the GUI Monitor

=over 4

=item &MonitorClient_Send($field,$val,$proc)

Cause the GUI monitor to display $val in field $field of this worker's
monitor pane.  Display occurs according to $proc, which can  be one of
the built-in monitor behaviors ("Log", "TimeChart", etc.) or a Tcl
procedure that defines a monitor extension. See the monitor
documentation for more details.

=back

=head2 Cache Access Methods

For all methods that access the cache (Clib_*), $headers should have the
following properties or 
badness 10000 will occur:

=over 4

=item *

Server headers (but not client headers) should begin with the server
status line, e.g. "HTTP/1.0 200 OK".

=item *

Headers should be separated by CR+LF.

=item *

A blank line (i.e. two consecutive CR+LF's) should end the headers.

=back

Data is returned as a scalar.  Status code is an integer, as defined
in F<clib.h>.

C<Clib_Push> pushes the given data and headers into the cache, and keys
them to the given URL.  The status codes returned are defined in
F<clib.h> and available as &CLIB_OK, etc. from Perl; generally, any
nonzero status indicates an error.

C<Clib_Query> checks whether something is in the cache without actually
retrieving it.  In the current implementation this requires a full TCP
transaction, so the time savings  compared to just fetching the data
outright depends heavily on how far away the data is.  It returns 
either &CLIB_CACHE_HIT or &CLIB_CACHE_MISS if the check succeeds, or
some error otherwise.

=cut

if (!defined $main::DEBUGGING && !defined $ENV{'CLIB_DEBUGGING'}) {

   bootstrap clib;

  # Preloaded methods go here.
  # Autoload methods go after __END__, and are processed by the autosplit program.

} else {

  ## Debugging mode:  emulate Clib_* functions

    use LWP;
    use MD5;

    $UA = new LWP::UserAgent or die "Couldn't create LWP::UserAgent";
    $UA->use_alarm(0);
    $UA->use_eval(1);
    #  create a working directory...

    $clib::username = (getpwuid($<))[0] || "user";
    $tmpdir = "/tmp/clib_debug.$username";
    unless (-d $tmpdir && -w $tmpdir) {
        mkdir $tmpdir,0777 or die "Failed to make directory $tmpdir: $!";
    }

    sub MonitorClient_Send {
        my($field,$val,$proc) = @_;
        warn "$field: $val \[$proc\]\n";
    }

    sub Options_Find {
        my $opt = shift;
        unless (defined %clib::Options) {
            my $filename = $ENV{'GM_OPTIONS'} || "gm_options.$clib::username";
            open(F, $filename)
                or open(F, $filename = "../$filename")
                    or die ("No options file $filename (even tried in cwd); " .
                            "set the GM_OPTIONS envariable to point to one\n");
            $clib::Options{"global.optfile"} = $filename;
            while (<F>) {
                chomp;
                $clib::Options{$1} = $2
                    if /^([^:]+):\s*(.*\S)\s*$/;
            }
            close F;
        }
        return $clib::Options{$opt} || "";
    }

        

    sub get_key {
        my $url = shift;
        return substr(MD5->hexhash($url),16);
    }

    sub Clib_Async_Fetch {
        return (&Clib_Fetch(@_))[0];
    }

    sub Clib_Fetch {
        my ($url,$hdrs) = @_;
        my $key = &get_key($url);

        if (-r "$tmpdir/$key" && -r "$tmpdir/${key}.h") {
            my ($data);
            open (F, "$tmpdir/${key}.h");
            $hdrs = '';
            $hdrs .= $_ while <F>;
            close F;
            open(F, "$tmpdir/$key");
            $data .= $_ while <F>;
            close F;
            return (0, $hdrs,$data);
        }
        if ($hdrs) {
            my %hdrs = split(/((?=^\S+):\s+)|([\r\n]+)/, $hdrs) ;
            $hdrs = new HTTP::Headers(%hdrs);
        } else {
            $hdrs = new HTTP::Headers;
        }
        my $req = new HTTP::Request ("GET",$url,$hdrs);

        my $result = $UA->request($req);
        my $ret_hdrs = join("\r\n", (join(' ', $result->code, $result->message)),
                            $result->headers_as_string)
            . "\r\n";
        my $data = $result->content;
        if ($result->is_success && $url !~ /^file:/) {
            &Clib_Push($url, $ret_hdrs, $data);
        }
        return (0, $ret_hdrs, $data);
    }

    sub Clib_Redir_Fetch {
	my ($url, $hdrs, $num_redirs) = @_;
	my @ret;

	if ($num_redirs <= 0) {
	    return &Clib_Fetch($url, $hdrs);
	}
	while($num_redirs--) {
	    @ret = &Clib_Fetch($url, $hdrs);
	    return @ret if ($ret[0] != 0);
	    $ret[1] =~ /^\S+ 3/ || return @ret;
	    if ($ret[1] =~ /\nlocation: ([^\r\n]*)/ios) {
		$url = 1;
	    } else {
		return @ret;
	    }
	}
	@ret;
    }

    sub Clib_Query {
        my ($url);
        my $key = &get_key($url);
        
        return ( -r "$tmpdir/$key" && -r "$tmpdir/${key}.h"
                ? 1                             # CACHE_HIT
                : 2                             # CACHE_MISS
                );
    }

    sub Clib_Push {
        my ($url,$hdrs,$data) = @_;
        my $key = &get_key($url);

        open(F, ">$tmpdir/$key");
        print F $data;
        close F;

        open(F, ">$tmpdir/${key}.h");
        print F $hdrs;
        close F;
    }

    sub InitializeDistillerCache {
	1;
    }

    sub make_argument {
	my ($type,$num,$val) = @_;
	$type.$num."=".$val;
    }

    sub Distill {
	my ($MimeType, $distiller, $Metadata, $Data, @args) = @_;
	my ($result, $outData, $outMetadata);
	my ($tfname);

	chomp $Metadata;
	print STDERR "***DISTILLER***\n\nHeaders:\n${Metadata}\n.\nArgs: ";
	print STDERR join(' ',@args), "\n";

	$tfname = "/tmp/distiller.$$";
	open(T, ">$tfname") or die "Cannot write to $tfname: $!\n";
	print T $Data;
	close(T);

	print STDERR "Data of type \"$MimeType\" written to \"$tfname\".\n" .
	    "Please distill it using the \"$distiller\" distiller\n" .
	    "and write the results to ".
	    "\"${tfname}.OUT\".\nWhen that is done, enter the new headers.\n" .
	    "End with \"#n\", where n is the return code:\n";
	$outMetadata = '';
	while(<STDIN>) {
	    if (/^\#(\d+)$/) {
		$result = $1;
		last;
	    }
	    $outMetadata .= $_;
	}
	$outData = '';
	open (T, "<${tfname}.OUT") or
	    warn "Could not read ${tfname}.OUT: $!\n";
	$outData = join('',<T>);
	close(T);

	unlink $tfname;
	unlink $tfname.".OUT";

	($result, $outData, $outMetadata);
    }

    sub DistillAsync {
	my ($MimeType, $distiller, $Metadata, $Data, @args) = @_;
	return (0, [$MimeType, $distiller, $Metadata, $data, @args]);
    }

    sub DistillRendezvous {
	my ($req, $timeout) = @_;
	&Distill(@$req);
    }

    sub DistillDestroy {
	my ($req) = @_;
	0;
    }
}

1;
__END__
