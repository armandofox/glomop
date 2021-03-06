#  Hey emacs, this is a -*- perl -*- program
#  A trivial distiller that converts keywords identified by arg id 9000 to be
#  red.
#

use TACCutils;

sub DistillerInit {
    @dictfiles = ("/var/tmp/nci/nci_all.dict", "/var/tmp/nci/cell_bio_all.dict");
    &BuildDicts(@dictfiles);
    0;                                      # the distOk return code
}

sub DistillerMain {
    my($data,$headers,$type,%args) = @_;
    my $flagch = chr(0164);
    my ($tmphtml,%tags) = &TACCutils::striphtml($data,1,0,1,$flagch);

# clear up some globals that we use over and over
    undef $fwd;
    undef $swd;
    undef %converted;
    undef $nextwd;
    undef $nextcapwd;
    undef $nextpair;
    undef $nextcappair;
    undef $nexttriple;
    undef $nextcaptriple;

#### make a backup of the data input to modify
    $modtext = $tmphtml;
    $modt2 = $tmphtml;

# begin the subsitution loop, wordwise over input
    while ($modt2 =~ m/\W*(\w+)\W*/g) {
	$nextwd = lc($1);
	$nextcapwd = $1;
#	print "Next word is $nextcapwd\n";
	
	if (defined($fwd)) {
	    $nextpair = lc($fwd . " " . $1);
	    $nextcappair = $fwd . " " . $1;
#	    print "  Next pair is $nextpair\n";
	    if (defined($swd)) {
		$nexttriple = lc($swd . " " . $fwd . " " . $1);
		$nextcaptriple = $swd . " " . $fwd . " " . $1;
#		print "    Next triple is $nextcaptriple\n";
	    }
	}


	# check in each dictionary for possible conversion
	foreach $dict (@dicts) {
	    # do single word conversions
	    if (exists $$dict{$nextwd}) {
		if (! exists $converted{$nextcapwd}) {
		    $convert = &DoAnchorSub($nextcapwd, $$dict{$nextwd});
#		print "$nextcapwd --> $convert\n";
                    if ($nextwd eq "cancer") {
                       $modtext =~ s/\s\Q$nextcapwd\E/ $convert/;
                    } else {
		       $modtext =~ s/\s\Q$nextcapwd\E/ $convert/g;
                    }
		    $converted{$nextcapwd} = "1";
		}
	    }

	    # do double word conversions
	    if ( (defined($nextpair)) &&
		(exists $$dict{$nextpair})
		) {
		if (! exists $converted{$nextcappair}) {
		    $convert = &DoAnchorSub($nextcappair, $$dict{$nextpair});
#		print "$nextcappair --> $convert\n";
		    $srchwd = $nextcappair;
		    # quote metacharacters, turn space into \s+
		    $srchwd =~ s/(\W)/\\$1/g;
		    $srchwd =~ s/(\\ )+/\\s\+/g;
		    # replace away!
		    $modtext =~ s/$srchwd/$convert/g;
		    $converted{$nextcappair} = "1";
		}
	    }


	    # do triple word conversions
	    if ( (defined($nexttriple)) &&
		(exists $$dict{$nexttriple})
		) {
		if (! exists $converted{$nextcaptriple}) {
		    $convert = &DoAnchorSub($nextcaptriple,
					    $$dict{$nexttriple});
#		print "$nextcaptriple --> $convert\n";
		    $srchwd = $nextcaptriple;
		    # quote metacharacters, turn space into \s+
		    $srchwd =~ s/(\W)/\\$1/g;
		    $srchwd =~ s/(\\ )+/\\s\+/g;
		    # replace away!
		    $modtext =~ s/$srchwd/$convert/g;
		    $converted{$nextcaptriple} = "1";
		}
	    }
	}

	# cycle the words captured from input
	$swd = $fwd;
	$fwd = $1;
    }
    
    # convert tags back
    $modtext =~ s/$flagch(\d+)/"$tags{$1}"/ge;

    # insert caveat at top
    $insert_string = "<font size=1>You are viewing this page with a test program developed by <a href=\"http://www.proxinet.com\">ProxiNet, Inc.</a> and the <a href=\"http://www.nih.nci.gov\">National Cancer Institute</a>'s Operation J-O-L-T. <a href=\"http://www.cs.berkeley.edu/~gribble\">Here</a> is how to remove the proxy information from your browser when you are finished.  Please send comments or questions to <a href=\"mailto:Bernard_Glassman\@nih.gov\"><i>Bernard_Glassman\@nih.gov</i></a></font><br><hr>";
    $modtext = &insert_at_top($modtext, $insert_string);
    # convert tags back
    $hdrs = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n";
    return($modtext, $hdrs, "text/html", 0);
}

1;

#### local functions

sub DoAnchorSub {
    my ($word, $url) = @_;

    $sub = "<a href=\"" . $url . "\">";
    $sub .= $word . "</a>";
    return $sub;
}

sub BuildDicts {
    my @dictnames = @_;
    my $num = 0;

  DICTFOREACH:
    foreach $name (@dictnames) {
	$dname = "dict" . $num;

	open(DICT, "$name") or next DICTFOREACH;
	print $_;
	while ($nextline = <DICT>) {
	    chomp($nextline);
	    $nextline = lc($nextline);
	    $nline2 = <DICT> or last DICTFOREACH;
	    chomp($nline2);
	    if (($nextline eq "") || ($nline2 eq "")) {
		last DICTFOREACH;
	    }
	    $$dname{$nextline} = $nline2;
#	    print "Added \"$nextline\" with \"$nline2\"\n";
	}
	$dicts[$num] = $dname;
	$num = $num + 1;
	close(DICT);
    }
}
