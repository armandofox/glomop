#!/usr/local/bin/perl
# $Id: excerpt.pl,v 1.1.1.1 1996/10/25 02:37:15 fox Exp $
#
# Excerpt relevant portions of FLEX generated C code
#

$opt = shift(@ARGV);

if($opt eq 'tables'){
    while(<>){
	($p = 1, print STDERR "**START: ", $_)
	    if /EXCERPT TABLES: START/;
	($p = 0, print STDERR "***STOP: ", $_)
	    if /EXCERPT TABLES: STOP/;

	($p = 1, print STDERR "**START: ", $_)
	    if /^\#define YY_END_OF_BUFFER /;
	($p = 0, print STDERR "***STOP: ", $_)
	    if /last_accepting_state/;

	$q1 = $1 if /yy_current_state >= (\d+)/;
	$q2 = $1 if /yy_current_state\] != (\d+)/;

	print if $p;
    }

    print "#define FLEX_MAGIC_STATE $q1\n";
    print "#define FLEX_MAGIC_STATE2 $q2\n";
}elsif($opt eq 'actions'){
    while(<>){
	($p = 1, print STDERR "**START: ", $_)
	    if /EXCERPT ACTIONS: START/;
	($p = 0, print STDERR "***STOP: ", $_)
	    if /EXCERPT ACTIONS: STOP/;

	($p = 1, print STDERR "**START: ", $_)
	    if /^#define INITIAL/;

	($p = 1, print STDERR "**START: ", $_)
	    if /^case 1:/;

	print if ($p && !(/yy_hold_char/ && /undo effects/));
    }
}else{
    die "usage: $0 tables|actions <foo.c >bar.c";
}

