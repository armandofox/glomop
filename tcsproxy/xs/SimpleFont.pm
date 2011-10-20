#  -*- perl -*- support for simple fonts on small clients
#

=head1 NAME

Font.pm - simple font support for non-scalable (bitmapped) fonts

=head1 SYNOPSIS

To be filled in

=head1 DESCRIPTION

This package provides a framework for specifying metrics for simple bitmapped
fonts.  Such font metrics are used by the L<Para> object, which defines
a set of simple font flags (bold, underline, etc.), to do
device-independent text layout and formatting.

Each device for which font metrics are provided needs to override this
framework.  For example, 'use PilotFonts' will give you the Palm Pilot
font metrics in the SimpleFont framework.

Each such package typically has an C<init_fonts> call that takes as an
argument the directory where font metric information for that device may
be found.

=cut

package SimpleFont;
require Exporter;
@ISA = qw(Exporter);

sub instvars {
    my $class = shift;
    foreach $var (@_) {
        if ($var =~ /(\S+):r/) {
            # read-only variable
            $var = $1;
            $action = qq!carp "Can't set instance variable `$var' this way, ignored"!;
        } else {
            $action = qq!\$self->{"$var"} = shift!;
        }
        eval "sub ${class}::${var}"  . qq!{
            my \$self = shift;
            $action if defined \$_[0];
            return \$self->{"$var"}; }! ;
    }
}

=head2 CONSTRUCTOR

=over 3

=item $result = &init_fonts($fontDir);

Initialize the font information for the given device, from the metrics
files in directory $fontDir.  Returns a true value on success; on
failure, may either C<die> or return a non-true value.

=item $fn = new SimpleFont ($name,$type,$first,$last,$maxwidth,$kernmax,$descent,$fwidth,$fheight,$offset,$ascender,$descender,$leading,$rowwords, $wx)

Create a new SimpleFont with the given metrics.  $wx should be a
reference to a 256-element array containing the pixel widths of the
characters in the ISO-8859-1 character set.  It is OK for characters not
in the font to have a width of C<undef>.  You shouldn't normally need to
do this by hand, because the C<init_fonts> call for a particular device
should create the appropriate SimpleFonts for you and export their
names.  Unfortunately, the way this is done varies from device to
device; see, for example, L<PilotFonts>.

=back

=cut

sub new {
    my($name,$type,$first,$last,$maxwidth,$kernmax,$descent,
       $fwidth,$fheight,$offset,$ascender,$descender,$leading,
       $rowwords, $wx) = @_;
    bless {
        'name' => $name,
        'type' => $type,
        'first' => $first,
        'last' => $last,
        'maxwidth' => $maxwidth,
        'kernmax' => $kernmax,
        'descent' => $descent,
        'fwidth' => $fwidth,
        'fheight' => $fheight,
        'offset' => $offset,
        'ascender' => $ascender,
        'descender' => $descender,
        'leading' => $leading,
        'rowwords' => $rowwords,
        'wx' => $wx,
    };
}


=head1 INSTANCE METHODS

=over 3

=item $h = $fn->height;

Return the height of a line of text, which is obtained by adding the
descender and ascender.

=cut

sub height {
    my $self = shift;
    return $self->descender + $self->ascender;
}

=item @wx = $fn->wx;   $wxref = $fn->wx;

Return the array of character widths for the font.  Not as useful as the
other methods, below.  If the caller wants an array, one is returned, otherwise
a reference to one is returned.  Caveat caller.

=cut

sub wx {  wantarray ? @{(shift @_)->{'wx'}} : shift->{'wx'};  }

=item $w = $fn->string_width($str);

Return the width in pixels of $str if it were rendered in font $fn.
Each character's width is determined from the width table.

=cut

sub string_width {
    my $self = shift @_;
    my $wx = $self->wx;
    my $len = 0;
    $_ = shift @_;                          # the string
    $len += $wx->[ord chop] while $_;
    $len;
}

=item @lines = $fn->split_string($str,$width[,$break[,$squeeze]])

Splits a string so that each line will fit in a space $width pixels wide.
Returns an array of substrings, one per line.  Characters
where the string is broken (spaces, etc.) are not included in the substrings,
so the concatenation of the substrings may not be identical to the original
string. 
If $break is given, it should be a regexp that matches the legal
break characters; by default, space and hyphen are break characters.  

Each element of the returned array will be a string containing at least one
character, even if the widest character in the string won't fit on one line.

=cut

sub split_string {
    my $self = shift;
    my $wx = $self->wx;
    my $str = shift || return ();
    my $origlen = length($str);
    my $width = shift;
    my $widest = $self->maxwidth;
    my $breaks = shift || ' ';
    my @lines = ();
    my ($lastbreak,$nextch);
    my ($est,$est_width);

    # while there is still text to be split, estimate the width of the string
    # that will fill the line, and keep adding chars until we actually hit the
    # boundary.  remember where the last breakable char was seen.

    $width = $widest if $width < $widest;
    $lastlength = 0;
    while (1) {
        $lastbreak = -1;
        $est = ($width / $widest)-1;
        $est_width = $self->string_width(substr($str,0,$est));
        while ($est_width <= $width
               && $est <= length($str)) {
            $nextch = substr($str,$est,1);
            $est++;
            $est_width += $wx->[ord $nextch];
            $lastbreak = $est if $nextch =~ /$breaks/;
        }
        if ($est <= length($str)) {
            # ok, we went over.  
            # If a break char was seen while scanning, break there.
            # Otherwise, break here.
            $est--;
            $lastbreak = $est if ($lastbreak == -1);
            #push(@lines, substr($str,0,$lastbreak));
            push(@lines, substr($str,0,$lastbreak));
            $str = substr($str,$lastbreak);
            # skip over leading break chars
            #$str =~  s/^${breaks}+//;
        } else {
            # this is the end, woohoo!
            #push(@lines, $origlen);
            push(@lines, $str);
            last;
        }
    }
    return @lines;
}

&SimpleFont::instvars('SimpleFont',
                      qw(type first last maxwidth kernmax descent
                         fwidth fheight offset ascender descender
                         leading rowwords));

sub init_fonts {
    die "init_fonts: I must be overridden by a subclass";
}

=back

=head1 BUGS

The array of widths should probably (also) be provided as a hash table,
where the keys are the ISO-8859-1 names of the characters.

=head1 COPYRIGHT

Copyright (C) 1997 by the Regents of the University of California.
This software is distributed under the terms of the GNU General Public
License, version 1.0 or greater.

=cut

1;
