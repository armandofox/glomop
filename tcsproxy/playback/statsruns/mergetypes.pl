#!/usr/sww/bin/perl5

#
# mergetypes.pl will take a file that has the format:
#    'type1' data1_1 data1_2 data1_3 ...\n
#    'type2' data2_1 data2_2 data2_3 ...\n
#    'type3' data3_1 data3_2 data3_3 ...\n
# and attempt to smoosh identical types together (such as GIF, gif, Gif, ...).
# For example, if 'type1' and 'type2' were the same ('typea'), it would smoosh it out
# as:
#    'typea' data1_1+data2_1 data1_2+data2_2 data1_3+data2_3 ...\n
#    'type3' data3_1         data3_2         data3_3         ...\n
# 
# run as:  ./mergetypes.pl < infile > outfile

@mt_types = (
	  [ "gif", "gif89", "tgif", "mgif", "gif\"", "giff", "cgif", "gif\'",
	  "gif2", "gif1", "gif%20", "gifhvs", "gifbg", "gifs", "gifa" ],
	  
	  [ "jpg", "jpeg", "jpg\"", "jpg\'", "jpg%20", "color-jpeg",  "jpeg\"",
	   "jpeg\'", "jpeg%20", "jpe" ],
	  
	  [ "pjpg", "pjpeg", "projpeg", "projpg" ],
	  
	  [ "html", "htm", "phtml", "htmx", "ohtml", "htmlx", "mhtml",
	  "html\"", "html\'", "rfhtml", "chtml", "htm\!", "htmll" ],
	  
	  [ "shtml", "html-ssi", "shtm" ],

	  [ "cgi", "cgi-bin", "cgibin", "acgi", "fcgi", "acgi$search", "scgi", "*cgi" ],

	  [ "map", "mapimage", "imagemap", "imap" ],

	  [ "gzip", "gz", "taz", "tgz" ],

	  [ "lha", "lharc" ],

	  [ "eps", "ps" ],

	  [ "man", "me", "ms" ],

	  [ "aif", "aifc", "aiff" ],

	  [ "mod", "nst" ],

	  [ "mid", "md", "midi" ],

	  [ "tif", "tiff" ],

	  [ "mpe", "mpeg", "mpg" ],

	  [ "mov", "qt" ],

	  [ "pl", "pl5", "perl" ],

	  [ "txt", "text" ]
	  );

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

while ($nextline = <STDIN>) {
    ($head, $tail) = ($nextline =~ /^\s*\'(.*)\'\s*(.*)$/) or die "failed match\n";

    chomp($tail);
    $righthead = &getRighthead($head);
    if (!defined(@$righthead)) {
	&addfirst($righthead, $tail);
    } else {
	&addmore($righthead, $tail);
    }
    $typeseen{$righthead}++;
}

# roll through array we've built up, and spit out results
local($,) = " ";
foreach $types (keys %typeseen) {
    my @typearr = @$types;

    print STDOUT "'$types' ";
    print STDOUT @$types;
    print STDOUT "\n";
}
local($,) = "";
1;

sub getRighthead {
    my $curhead = shift(@_);
    my $arr2;

    foreach $arr2 (@mt_types) {
	my $firstel = $$arr2[0];
	my $els;
	
	foreach $els (@$arr2) {
	    if ($els eq $curhead) {
		return $firstel;
	    }
	}
    }
    return $curhead;
}
sub addfirst {
    my $righthead = shift(@_);
    my $tail = shift(@_);
    my @tmparr = split(/\s+/, $tail);

    push(@$righthead, @tmparr);
}

sub addmore {
    my $righthead = shift(@_);
    my $tail = shift(@_);
    my @tmparr = split(/s+/, $tail);
    my $i;
    my $num = scalar(@$righthead);

    for ($i=0; $i<$num; $i++) {
	$$righthead[$i] += $tmparr[$i];
    }
}
