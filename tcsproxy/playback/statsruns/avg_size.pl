#!/usr/sww/bin/perl5

$totsize = 0;
$numcount = 0;

while ($nextline = <>) {
    ($bucksize, $count) = ($nextline =~ /^(\d+)\D+(\d+)$/);

    if ($bucksize < 500000000) {
	$numcount += $count;
	$totsize += $count * $bucksize;
    }
}

$avgsize = $totsize / $numcount;
print "num: $numcount  totsize: $totsize  avgsize: $avgsize\n";

