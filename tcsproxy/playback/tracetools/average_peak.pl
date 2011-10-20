#!/usr/sww/bin/perl5

$total = 0;
$count = 0;
$max = 0;
while ($nextline = <STDIN>) {
    chomp($nextline);
    ($bucket, $num) = ($nextline =~ /^(\S+)\s+(\S+)$/);

    if ($num > $max) {
	$max = $num;
    }
    $total += $num;
    $count += 1;
}

$mean = $total / $count;
print "total: $total  mean: $mean  max: $max\n";
