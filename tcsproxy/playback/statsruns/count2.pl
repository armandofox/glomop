#!/usr/sww/bin/perl5

$tot = 0;

while ($nextline = <>) {
  ($a, $b) = ($nextline =~ /^'(.*)'\s+(\S+)\s+/);

  $tot += $b;
}

print "total is $tot\n";
