#
#  A "Dispatcher" that allows users to specify static chains of multiple
#  workers, overriding the workers' own X-Route return values.
#

use TACCutils;
use clib;

sub DistillerInit {
    $ChangeFormArgId = 'i99';                   # set i99=1 to trigger the change form

    #  use lowercase only in the following table!!!!

    %mimeTable = qw(text/html                       s100
                    image/gif                       s101
                    image/jpg                       s102
                    image/jpeg                      s102
                    text/plain                      s103
                    application/octet-stream        s104
                    image/tbmp                      s105
                    image/tbmp2                     s106
                    );

    # open the config file and read the names of all the distillers.

    # BUG::there should be an Options_Find that takes a wildcard, i.e. all
    # lines of the form "dist.*", or an Options_Keys that takes a wildcard and
    # returns its expansion into a list of distinct option keys.

    @Dists = ();
    &refresh_distiller_list;
    $SIG{'HUP'} = \&refresh_distiller_list;

    &message("Dispatcher running\n");
    return 0;
}

sub message {
    &MonitorClient_Send("Dispatcher messages", shift @_, "Log");
}

sub refresh_distiller_list {
    my $optfile = &Options_Find("global.optfile");
    die "$optfile not readable!"
        unless -r $optfile;

    open(O,$optfile) or die "Can't open $optfile: $!";
    @Dists = ();
    while (<O>) {
        push(@Dists, $1), &message("Adding distiller: $1\n")
            if /^dist\.([^:]+)/;
    }
}
    
sub DistillerMain {
    my ($data,$hdrs,$type,%args) = @_;      # only $hdrs really meaningful
    my $argid;
    my $route;
    my $retcode = 10;
    my $nhdrs = &make_headers($hdrs);
    my $url = &get_my_url($hdrs);
    
    &message("Request for $url\n");

    # if this request already has an X-Static-Route header, it is an error for
    # the request to have been passed to this dispatcher!

    if ($route = $nhdrs->header("X-Static-Route")) {
        return(&htmlify("<h1>Error: Multiple dispatch</h1> " .
                        "URL <i>$url</i> dispatched more than once!<p>" .
                        "Last static route is: [$route]",
                        "TranSend Dispatching Error", "bgcolor=#ff0000"),
               "", "text/html", 0);
    }

    # is this a request to change the preferences?
    if ($args{$ChangeFormArgId} == 1) {
        my $form = &create_change_form(\%args);
        $data = &htmlify($form,
                         "Change TranSend static routing",
                         "BGCOLOR=#ffffff");
        $nhdrs = &make_headers('');
        $nhdrs->header('Pragma' => 'no-cache');
        $nhdrs->header('Content-type' => 'text/html');
        $hdrs = $nhdrs->as_string;
        $retcode = 0;
        $type = "text/html";
    } elsif (defined ($argid = $mimeTable{lc $type})
             && ($route = $args{$argid})) {
        # return X-Static-Route
        $route = &unescape($route);
        &message("Setting static route: $route\n");
        $nhdrs->header("X-Static-Route", $route);
        $hdrs = $nhdrs->as_string;
        #$hdrs= &add_header($hdrs, "X-Static-Route", $route);
    } else {
        # no redispatch since we have no route to recommend
        $retcode = 0;
    }
    return ($data,$hdrs,$type,$retcode);
}

sub create_change_form {
    my $argsref = shift;
    my $dists = "<ul><li>" . join("\n<li>", @Dists) . "\n</ul>";
    my $html = <<EndOfDescription;

<h2>Change Routing</h2>

To change the route associated with a particular MIME type, type the
desired route separated by semicolons into the appropriate field.
<b>(Don't use spaces!)</b>  To
<i>delete</i> a route, leave the appropriate field blank.<p>

(If you want some brownie points, spruce up this page using JavaScript
so that you can click, drag, etc. to specify chains, rather than having
to type or mouse-paste into the text areas.  You'll find the code in
<tt>perl_dir/Dispatcher.pl</tt> in the common code area.)<p>

The names of filters currently installed are:

$dists
EndOfDescription

    my @fieldlist = ();
    my $k,$v;
    while (($k,$v) = each %mimeTable) {
        push(@fieldlist,
             lc($k), $v, "text", undef);
    }

    return $html . &make_prefs_form(@fieldlist,$argsref);
}

1;
