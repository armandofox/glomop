#!/usr/sww/bin/perl5

#
# sepknown.pl will take a file that has the format:
#    'type1' data1_1 data1_2 data1_3 ...\n
#    'type2' data2_1 data2_2 data2_3 ...\n
#    'type3' data3_1 data3_2 data3_3 ...\n
#
# and will report those types that it knows about (in the @mt_known array),
# but add all the others to a type 'others'.
# 
# run as:  ./sepknown.pl < infile > outfile

@mt_known = (
	  "gif", "jpg", "pjpg", "html", "shtml", "cgi", "map", "gzip",
	  "lha", "ps", "man", "aif", "mod", "mid", "tiff", "mpeg",
	  "mov", "zip", "Z", "hqx", "sea", "arc", "txt",
	  "doc", "pdf", "dvi", "tex", "rtf", "mif", "au", "wav",
	  "snd", "sf", "iff", "voc", "ul", "fssd", "xbm", "xpm",
	  "rgb", "ppm", "pgm", "pbm", "xwd", "ras", "pnm", "avi", "movie",
	  "class", "pl", "exe", "dat", "so", "dll", "asp", "gw", "xp", "doc",
	  ""    # this last one is the NULL-file suffix
	  );	  


# roll through the input, classifying as we go
foreach $foo (@mt_known) {
    $hs_known{$foo} = 1;
}

while ($nextline = <STDIN>) {
    ($head, $tail) = ($nextline =~ /^\s*\'(.*)\'\s*(.*)$/) or die "failed match\n";

    chomp($tail);
    if ($hs_known{$head} == 1) {
	print STDOUT "'$head' $tail\n";
    } else {
	&addmore($tail);
    }
}

local($,) = " ";
print STDOUT "'other' ";
print STDOUT @others;
print STDOUT "\n";

local($,) = "";
1;

sub addmore {
    my $tail = shift(@_);
    my @tarr;
    my $i;
    my $count=0;
    my $next;

    chomp($tail);
    @tarr = split(/\s+/, $tail);
    $count = scalar(@tarr);
    for ($i=0; $i<$count; $i++) {
	$others[$i] += $tarr[$i];
    }
}
