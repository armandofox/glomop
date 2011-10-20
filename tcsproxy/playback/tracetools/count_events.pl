#!/usr/sww/bin/perl5

##
## count_events.pl - this script accepts as arguments a bucket size (in
## seconds) and a minimum time (in seconds), and sucks from standard input
## a series of trace events.  It subtracts the minimum time from the
## client request time in order to produce the relative time of the event,
## and then adds an event to the appropriate bucket.  It will
## output a histogram of events at the end.
##

$bucketsize = shift @ARGV or die "usage: count_events.pl bucketsize mintime";
$mintime = shift @ARGV or die "usage: count_events.pl bucketsize mintime";

$maxbucket = 0;

while ($nextline = <STDIN>) {
    ($reqsec, $requsec) = ($nextline =~  /^(\d+)\:(\d+) .*$"/);
    $timeval = $reqsec - $mintime;
    $bucketval = int($timeval / $bucketsize);
    $bucket[$bucketval]++;
    if ($bucketval > $maxbucket) {
	$maxbucket = $bucketval;
    }
}

$valid = 0;

for ($i=0; $i<$maxbucket; $i++) {
    if (defined($bucket[$i])) {
	$valid = 1;
	print "$i $bucket[$i]\n";
    } else {
	if ($valid) {
	    print "$i 0\n";
	}
    }
}

exit(0);


