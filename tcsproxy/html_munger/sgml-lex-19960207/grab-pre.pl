#!/usr/local/bin/perl
#
# This is a HACK!
#

while(<>){
    $p = 1 if s/.*<PRE\s*>//i;
    if(s,</PRE\s*>.*,,i){
	print "\n<!-- reset parser \" ]> -->\n\n";
	$p = 0;
    }

    s/&lt;?/</g;
    s/&gt;?/>/g;
    s/&quot;?/"/g;
    s/&amp;?/&/g;

    print if $p;

}
