#!/usr/sww/bin/perl5

# suck in the sizes array

open(SZBUCK, "sz_buckets.txt");
while ($nextline = <SZBUCK>) {
    ($off, $sz) = ($nextline =~ /^(\S+)\s+(\S+)$/);
    $szarr[$off] = $sz;
}
close(SZBUCK);

# now for each line of the sizes array, do the nasty

while ($nextline = <>) {
    @sizes = split(/\s+/, $nextline);
    $len = scalar(@sizes);
    $oftype = substr($sizes[0], 1, length($sizes[0])-2);
    $of = ">" . $oftype . ".sizestats";
    open(OF, $of);
    for ($i=1; $i<$len; $i++) {
	$os = $szarr[$i-1] . " " . $sizes[$i] . "\n";
	print OF $os;
    }
}
