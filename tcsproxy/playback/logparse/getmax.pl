#!/usr/sww/bin/perl5

$gmax = 0;
$gmin = 99999999;

while ($nextline = <>) {
  ($time, $avg, $std, $min, $max) = 
       ($nextline =~ /^(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)$/);
  if ($gmin > $min) { $gmin = $min;  print "min now $min\n"; }
  if ($gmax < $max) { $gmax = $max;  print "max now $max\n"; }
}

print "min: $gmin   max: $gmax\n";
1;

