#!/usr/sww/bin/perl5

while ($nextline = <STDIN>) {

    # Get line of stats
    ($accesses, $misses, $cng_miss, $sng_miss,
     $ims_miss, $c_a_e, $c_a_r, $tst, $tlt, $cne, $ccs) =
	 ($nextline =~
	  /^(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)$/);

    print "got $accesses $misses\n";
    if ($accesses == 0) {
	;
    } else {
	$hitrate = 1.0 - ($misses/$accesses);
	$timed = $tlt - $tst;
	
	print "$timed $hitrate\n";
    }
}

