#!/opt/bin/perl -w
#
#  Unpacks a tSTR resource from a file into the "trivial interchange" format
#  that the Tcl/Tk layout client understands.
#

# break_string should be a unique expression (ie that should "never occur" in
# the body text of an html page) that this script will use to denote where it
# thinks line breaks are indicated in the output.  

$break_string = '<//>';                     # not a great choice....

BEGIN {
    $main::DEBUGGING = 1;
    push(@INC,
         qw(/disks/barad-dur/now/pythia/release/lib/perl-5.002
            ../xs/lib
            ../xs/lib/i586-linux/5.003
            ));
}

$Debug = 1 if grep(/^-d/, @ARGV);
sub debug { warn @_ if $Debug; }

$proxyHome = &Options_Find("proxy.home")
    or die "No proxy.home option in config file\n";
$Tbmp = "$proxyHome/bin/Tbmptopnm";
die "No Tbmptopnm executable in $Tbmp\n"
    unless -x $Tbmp;
use clib;
use Para;

$buf = '';
if ($fn = pop @ARGV) {
    open(F, $fn) or die $!;
    $buf .= $_ while $_=<F>;
    close F;
} else {
    $buf .= $_ while <>;
}

&debug("Buffer length is " . length($buf) . "\n");

$i = 0;
while ($buf) {
    $link = '';
    $plen = unpack("n",$buf);
    last if $plen == 0xffff;
    ($back,$for,$x,$y,$xe,$ye,$atr) = unpack("n7",$buf);
    $buf = substr($buf,14);
    $linklen = 0;
    if ($atr & $F_LINK) {
        # extract link info
        $linklen = unpack("n",$buf);
        $buf = substr($buf,2);
        $link = unpack("A$linklen", $buf);
        chop $link if $link =~ /\000$/;
        $linklen++ if $linklen & 1;
        $buf = substr($buf,$linklen);
        $linklen += 2;
    }
    if ($atr & $F_LINK_ALT) {
        # extract alternate-link info
        # warn "Extracting LINK_ALT\n";
        $altlinklen = unpack("n",$buf);
        $buf = substr($buf,2);
        $altlink = unpack("A$altlinklen", $buf);
        chop $altlink if $altlink =~ /\000$/;
        $altlinklen++ if $altlinklen & 1;
        $buf = substr($buf,$altlinklen);
        $linklen += $altlinklen + 2;
    }
    if ($atr & $F_BITMAP) {
        &debug("Bitmap #$i:");
        $filename = "/tmp/$$.$i.ppm";
        $data = "[image $filename]";
        open(PROG, "| $Tbmp -2bit>$filename")   or die $!;
        $i++;
        print PROG substr($buf,0,$for-14-$linklen);
        close PROG;
        unlink "/tmp/$$.$i.ppm";
        $buf = substr($buf,$for-14-$linklen);
    } else {
        $data = '';
      FRAG:
        while (1) {
            $len = unpack("n",$buf);
            $buf = substr($buf,2);
            &debug("Data EOS:"), last FRAG if $len == 0;
            &debug("Data length $len:");
            $data .= unpack("A$len",$buf) . $break_string;
            $len++ if $len & 1;
            $buf = substr($buf,$len);
        }
        
    }
    printf("%4d %4d %4d %4d %4d %s {%s}\n", $x,$y,$xe,$ye,$atr,
           $link, $data);
    # verify that forward-link was correct
}

