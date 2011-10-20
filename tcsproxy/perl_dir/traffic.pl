#
#  "Web traffic light" filter.
#  Shows a red or green light to indicate whether a given link is in the cache.
#

use clib;
use TACCutils;

sub DistillerInit {
    &MonitorClient_Send("traffic.pl", "Traffic lights ready", "Log");
    0;
}

sub DistillerMain {
    my($data,$hdrs,$type,%args) = @_;

    my $greenlight = '<img border=0 src="http://www.cs.berkeley.edu/~fox/blueball.gif">';
    my $redlight = '<img border=0 src="http://www.cs.berkeley.edu/~fox/redball.gif">';

    my ($what,$thing,$attrs);
    my $newdata = '';
    my $baseurl = &get_my_url($hdrs);
    
    while ($data) {
        ($what,$thing,$attrs) = &next_entity($data);

        $newdata .= ($what == 1 ? $attrs->{'_all_'} : $thing);
        next unless ($what == 1
                     && $thing eq 'a'
                     && defined($attrs->{href}));              # ignore all but tags

        # see if the link is in the cache.

        # aack!  Clib_Query is temporarily busted....
        #$result = &Clib_Query(&canonicalize($attrs->{href}, $baseurl));
        $result = (rand(1) > .5);

        if ($result == 1) {                 # cache hit
            $newdata .= $greenlight;
        } else {
            $newdata .= $redlight;
        }

    }
    return ($newdata, "", "text/html", 0);
}

1;

        
        
