#!/usr/sww/bin/perl5

#
# sep3.pl will take a file, and produce three files out of it.  The first file
# will contain all files with lines that are 0 mod 3, the second 1 mod 3, and
# the third 2 mod 3.
#
# run as:  ./sep3.pl file1 file2 file3 < infile
# 

&parseArgs;

while (1) {
    if (!($next1 = <STDIN>)) {
	&finishUp(0);
    }
    if (!($next2 = <STDIN>)) {
	print STDERR "Incomplete triplet (1 of 3)\n";
	&finishUp(1);
    }
    if (!($next3 = <STDIN>)) {
	print STDERR "Incomplete triplet (2 of 3)\n";
	&finishUp(1);
    }

    print FILE1 $next1;
    print FILE2 $next2;
    print FILE3 $next3;
}

1;

sub parseArgs {
    $file1 = shift @ARGV || &usage;
    $file2 = shift @ARGV || &usage;
    $file3 = shift @ARGV || &usage;

    open(FILE1, ">$file1") or die "Couldn't open $file1 for writing.\n";
    open(FILE2, ">$file2") or die "Couldn't open $file2 for writing.\n";
    open(FILE3, ">$file3") or die "Couldn't open $file3 for writing.\n";    
}
sub usage {
    print "Usage: ./sep3.pl file1 file2 file3 < infile\n";
    exit(1);
}

sub finishUp {
    $exstat = shift(@_);
    close(FILE1);
    close(FILE2);
    close(FILE3);
    exit($exstat);
}
