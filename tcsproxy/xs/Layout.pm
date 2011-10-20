#
# layout.pl - simple layout packer
# $Id: Layout.pm,v 1.17 1997/10/28 02:30:17 fox Exp $
#

package Layout;
require "util.pl";
use Para;
@ISA = qw(Exporter);

=head1 NAME

Layout.pm - simple layout packer

=head1 SYNOPSIS

 $ly = new Layout ($x0,$y0,$x1,$y1[,$grid]);

 ($x0,$y0,$x1,$y1) = $ly->pack_static($para,$halign,$valign);
 $leftover_para = $ly->pack_dynamic($textpara,$measproc,$halign,$valign);

 $ly->clear();
 $ly->clearleft();
 $ly->clearright();
 $ly->clearboth();

=head1 DESCRIPTION

The layout manager takes an initial bounding box upper left and lower right
corner  (x0,y0,x1,y1) and packs flowable (e.g. text) and non-flowable
(e.g. images) objects into it.

To pack an object, you call one of two packing functions with the
Para
containing the object.  The packing information for the Para -- its
bounding box coordinates relative to the initial layout coordinates -- 
is filled in the Para's x, y, width, and height fields.

=cut

=head1 CONSTRUCTOR

=over 3

=item $ly = new Layout $x0,$y0,$x1,$y1[,$hgrid,$vgrid]

Create new layout object with initial bounding box $x0,$y0 (upper left), ,$x1,$y1
(lower right).  If $hgrid and $vgrid are given, statically-packed
objects' bounding boxes are rounded up to the nearest multiple of these
sizes in the horizontal and vertical dimensions; they default to 1.

=back

=cut

&util::instvars('Layout',
                qw(thresh left right top bottom stack hgrid vgrid)
                );

sub new {
    my $class= shift;
    my ($x0,$y0,$x1,$y1,$hgrid,$vgrid) = @_;

    bless {
        "left" => $x0,
        "right" => $x1,
        "top" => $y0,
        "bottom" => $y1,
        "hgrid" => $hgrid || 1,
        "vgrid" => $vgrid || 1,
        # "stack" => [[$x0,$y0,$x1,$y1],[$x0-1,$y0-1,$x1+1,$y1+1]],
        "stack" => [[$x0,$y0,$x1,$y1], [$x0,$y0,$x1,$y1]],
    };
}

=head1 INSTANCE METHODS

=over 3

=item $ly->pack_static($para,$halign,$valign[,$thresh])

Packs the specified para according to constraints $halign and $valign, each of
which may be "left", "top", "right", "bottom", or -1, 0, 1 respectively.  Note
that "left" means "top" if given as the $valign argument; similarly
"right"; and vice versa.  The object will not be "flowed" around other
objects.

The object's para metadata should be of the form "NxM" where N and M are
integers giving the pixel dimensions (width,height) of the para's bounding box.

Returns: a list of coordinates (left,top,right,bottom) if success;
-1, if the available space will not accommodate the object; a string
describing an error, otherwise.  The para's x, y, width,and height fields
will be set if the object was packed successfully.

=cut

sub pack_static {
    my ($self,$para,$halign,$valign,$thresh) = @_;
    $halign = -1 unless defined $halign;
    $valign = -1 unless defined $valign;
    $thresh = $self->thresh unless defined $thresh;
    
    &util::debug('layout', "pack_static `" , $para->otext,
                 "' halign=$halign valign=$valign");
    &util::debug('layout', ">>> ", $self->print_stack);

    #$self->print_stack;

    my ($x,$y);

    $x = $para->width;
    $y = $para->height;

    # enforce gridding if necessary
    $x++ while ($x % $self->hgrid);
    $y++ while ($y % $self->vgrid);

    my($cl,$ct,$cr,$cb) = @{$self->spop()};
    # try merging frames
    if ($self->stacksize > 0) {
        my($pl,$pt,$pr,$pb) = $self->tos();
        if (($pl == $cr || $pr == $cl)
            && ($pt == $ct)) {
            &util::debug('layout', "Merging frames");
            $self->spop();                  # get rid of old pushframe
            $cl = &util::min($cl,$pl);
            $cr = &util::max($cr,$pr);
            $cb = &util::max($cb,$pb);
        }
    }
        
    &util::debug('layout', "Currentframe is $cl $ct $cr $cb");

    # make sure it's not too wide.
    if ($x > ($cr-$cl)) {
        $self->spush( [$cl,$ct,$cr,$cb] );
        &util::debug('layout', "pack_static returns: out of room!");
        return -1;
    }

    # but what if it was too tall?
    if (0 && $y > ($cb-$ct)) {
        my($pl,$pt,$pr,$pb) = $self->tos();
        if ($cb == $pt && $pl <= $cl && $cr <= $pr) {
	    ## We can steal some vertical space from the box below us
            &util::debug('layout', "Stealing vspace");
            $self->spop();                  # get rid of old pushframe
	    $self->spush( [$pl,$ct+$y,$pr,$pb] );
            $cb = $ct + $y;
        }
    }

    # invariants:
    # 1. top of stack is the bounding box to use.
    # 2. next elt on stack is the push frame.

    # pack the object.


    # if it's not the same width as the current bounding box, compute the new
    # coords.
    my($fl,$fr,$ft,$fb);                    # left right top bottom
    if ($halign == -1) {                    # left
        $fl = $cl;
        $fr = $cl+$x;
    } elsif ($halign == 0) {
        $fl = ($cl+$cr-$x) >> 1;
        $fr = $fl+$x;
    } else {
        $fr = $cr;
        $fl = $cr-$x;
    }
    # BUG::no valign for now; assume top
    if (1 || $valign == -1) {                    # top
        $ft = $ct;
        $fb = $ct+$y;
    }

    if ($fb > $cb  &&  ($cr-$fr > $thresh)) {
        # problem! the item being packed is taller than the cavity (and 
        # is not the rightmost item on the line).
        # stretch the cavity vertically, stretch the neighbor cavity (next
        # guy on stack) to the same spot vertically.  Note that this will
        # still give the desired behavior for flush-right items.
        $cb = $fb;
        my($nl,$nt,$nr,$nb) = @{$self->spop()};
        $self->spush([$nl,&util::max($nt,$fb),$nr,&util::max($nb,$fb)]);
    }
      
    
    # calculate new top and bottom.
    # if we're too close to  edges, it's time to start a new "row".

    if ($halign == 0 || ($cr-$fr <= $thresh) && ($fl-$cl <= $thresh)) {
        #if ($self->stacksize == 0) {
        #   $self->spush( [$cl,$ct,$cr,$cb] );
         #   return -1;
        #}
        #my($pl,$pt,$pr,$pb) = @{$self->spop()};
        my($pl,$pt,$pr,$pb) = ($cl,$ct,$cr,$cb);
        if ($fb < $pt) {
            $self->spush( [$pl,$pt,$pr,$pb] );
            $self->spush( [$fl,$fb,$fr,$pt] );
        } else {
            $self->spush( [$pl,$fb,$pr,$pb] );
            if ($fl != $pl || $fr != $pr) {
                $self->spush( $halign == -1
                             ? [$fr,$pt,$pr,$fb]
                             : [$pl,$pt,$fl,$fb] );
            }
        }
    } else {
        # not all of the horiz. space will be used up.  If this is the first
        # item laid out in this packing cavity, we must push a new pushframe.
        # Update nextframe to reflect the horizontal space this packing
        # operation used up.
        if (($halign == -1 && $fl == ($self->tos)[0])
            || ($halign == 1 && $fr == ($self->tos)[2])) {
            #$self->spop();
            $self->spush([$cl,$fb,$cr,$cb]);    # new pushframe
            # BUG::do we want to check here for degenerate case of zero height
            # or zero width frame being pushed?  (they'll just get discarded
            # anyway when nothing fits in them)
        }
        $self->spush(($halign == -1)
                     ? [$fr,$ct,$cr,$fb]
                     : [$cl,$ct,$fl,$fb]);  # new nextframe
        #unshift(@{$self->{"lastbottom"}}, $fb);
    }

    &util::debug('layout', "<<< ", $self->print_stack);
    $self->{"top"} = $fb if $self->{"top"} < $fb;
    $para->width($fr-$fl);
    $para->height($fb-$ft);
    $para->x($fl);
    $para->y($ft);
    &util::debug('layout', "pack_static returns $fl,$ft,$fr,$fb");
    return ($fl,$ft,$fr,$fb);
}

=item  *

 ($status,$leftover) = $ly->pack_dynamic($text,$halign,$valign[,$hthresh,$vthresh]])

Dynamically packs the text to fill as much vertical space as corresponds
to the bottom boundary of the pushframe.
$text should be a Para.  Returns the text that was
left over (as another Para), which is not necessarily a
substring of the original text, since tags that are open at the time the text
is cut off need to be closed and reopened across text chunks.

$hthresh and $vthresh are threshold: if the remaining horizontal (verticlal)
space in the packing cavity 
is narrower (shorter) than this amount, no attempt should be made to pack the
text.  If $vthresh and $hthresh are
not given, the current threshold value for the layout is used.

@restargs, if supplied, will be passed to the text measurement function as
described below.

The Para passed in ($text) will have its width, height, x, and y fields
modified to 
reflect where it was laid out.    

pack_dynamic returns a status code and possibly also a Para.  If the status
code is 1, all of the text was successfully packed.  If the status
code is 0, part of the text was successfully packed; the Para
returned contains the remaining text (that did not fit).  You can
then call pack_dynamic again with this leftover text, and so on.

If the status code is -1 (-2), packing was not attempted because there
wasn't enough room in the next available packing cell given the specified
horizontal (vertical) thresholds.  If this happens, you could (for example)
call L<clearleft> to open some room, and try again.

=cut

sub pack_dynamic {
    my($self,$text,$halign,$valign,$hthresh,$vthresh) = @_;

    $hthresh = 0 unless defined $hthresh;
    $vthresh = 0 unless defined $vthresh;
    
    my $newpara = undef;

    # invariants:
    # 1. top elt of "lastbottom" stack is the cutoff point we're aiming for

    my $packregion = $self->{"stack"}->[0];

    
    # determine how much text is left over if we pack a box of that height and
    # the width of the packing cavity.

    my $boxwidth = $packregion->[2] - $packregion->[0];
    #my $boxht = $bottom - $packregion->[1];
    my $boxht = $packregion->[3] - $packregion->[1];

    if ($boxwidth < $hthresh) {
        &util::debug('layout', "packregion width $boxwidth less than $hthresh");
        return (-1, undef);
    }
    if ($boxht < $vthresh) {
        &util::debug('layout', "packregion height $boxht less than $vthresh");
        return (-2, undef);
    }

    &util::debug('layout', "pack_dynamic `" . $text->otext .
                 "' halign=$halign valign=$valign");

    my $lht = $text->font->height;
    my @lines = $text->font->split_string($text->otext, $boxwidth, " ");
    $text->{'lines'} = \@lines;
    my $fitlines = &util::min(int($boxht/$lht), ($#lines+1));
    
    $boxht = $fitlines * $lht;

    if ($fitlines < ($#lines+1)) {
        #  Return "didn't fit" if the text overflows this line and there is no
        #  place to word-break before the overflow.
        if ($lines[0] !~ /\s$/) {
            &util::debug('layout', "Punting on " . $text->otext);
            return (-1,undef);
        }
        $newpara = $text->clone();
        $newpara->otext(join('', @lines[$fitlines..$#lines]));
        $text->otext(join('', @lines[0..$fitlines-1]));
        &util::debug('layout', "Splitting: " . $text->otext . "//" .
                     $newpara->otext);
    } else {
        # all lines were packed successfully.  If the last line is an "orphan"
        # line (ie not wide enough to fill the packing cavity, return it as a
        # leftover so it wil be packed separately.  If this is the *only* line
        # and it is an orphan, reduce the width of the packing box.

        my $lastlinewidth = $text->font->string_width($lines[$#lines]);
        if ($boxwidth - $lastlinewidth  > $hthresh) {
            if ($#lines == 0) {
                # this is the only line, reduce packing cavity
                &util::debug('layout', "Single orphan line `"
                             . $text->otext . "'");
                $boxwidth = $lastlinewidth;
            } else {
                &util::debug('layout', "Chopping orphan line `$lines[$#lines]'");
                $newpara = $text->clone();
                $newpara->otext($lines[$#lines]);
                $text->otext(join('', @lines[0..$#lines-1]));
                $boxht -= $lht;
            }
        } else {
            $newpara = undef;
        }
    }

    # actually pack the thing.
    $text->width($boxwidth);
    $text->height($boxht);
    &util::debug('layout', "Calling pack_static for $boxwidth x $boxht");
    my($l,$t,$r,$b) = $self->pack_static($text,$halign,$valign);
    &util::debug('layout', $self->print_stack);
    return (-1,undef) if $l == -1;                  # out of space!
    $text->width($r-$l);
    $text->height($b-$t);
    $text->x($l);
    $text->y($t);
    return (defined ($newpara) ? (0,$newpara) : (1,undef));
}

=item $ly->grow_frame_to_atleast($hpixels)

Grow the topmost stack frame to be at least $hpixels pixels wide.  If
it's already wide enough, it's not changed.  Either of these means success
(true return value).  Failure means the frame couldn't be grown.

=cut

sub grow_frame_to_atleast {
    my $self = shift;
    my $hpixels = shift;

    my @stktop = $self->tos();
    my ($width,$stktop);
    return undef if $stktop[0] == -1;
    if (($width = $stktop[2]-$stktop[0]) < $hpixels) {
        $stktop = $self->spop();
        $stktop->[2] += $hpixels-$width;
        $self->spush($stktop);
    }
    return 1;
}

sub spush { unshift(@{$_[0]->{"stack"}}, $_[1]) }
sub spop { shift(@{$_[0]->{"stack"}}) }
sub stacksize { scalar @{$_[0]->{"stack"}} }
sub tos { $#{$_[0]->{"stack"}} >= 0
              ? @{$_[0]->{"stack"}->[0]}
          : (-1,-1,-1,-1) }

=item $ly->skip($vpixels)

Insert a conceptual line break after the last object packed, by skipping
$vpixels pixels in the vertical direction.  Useful if
you just packed an object flush left (or right) which was not the full
width of the available packing area, and then you want to pack another
object below it.  Gives the effect (roughyl) of <BR> in HTML.  Returns
true iff success. 

=cut

sub skip {
    my $self = shift;
    my $skip = shift || return 1;
    my @packregion = $self->tos;

    # create "fake" paragraph that will fill out the horizontal space in the
    # packregion and consume as much vertical space as specified

    my $p = new Para;
    $p->width($packregion[2]-$packregion[0]);
    $p->height($skip);

    if ($self->pack_static($p,-1,0) != -1) {
        return 1;
    }
    if ($self->stacksize > 2) {
        $self->spop();
        return 1;
    } else {
        return undef;
    }
}

=item $ly->clearleft

=item $ly->clearright

=item $ly->clearboth

Arrange for the next object to be packed flush against the page
left, right, or both margins.  Gives you the functionality of <BR CLEAR=LEFT>,
for example.  Returns true iff success.

=cut

sub clearleft {
    return &cleartoleft(@_,0);
}

sub cleartoleft {
    my $self = shift;
    my $left = shift;
    my $height = shift;
    my $right = $self->right;

    if (($self->tos)[0] == $left) {
        # skip instead
        $self->skip($height);
    } else {
        $self->spop() while ($self->stacksize > 0
                             && (($self->tos)[0] != $left));
        if ($self->stacksize == 0) {
            $self->spush( [$self->left,$self->top,$self->right,$self->bottom] );
        }
    }
    &util::debug('layout', "Clearleft, next frame is " .
                 join(',',$self->tos));
    return 1;
}

sub clearright {
    my $self = shift;
    my $right = $self->right;
    my $left = $self->left;

    $self->spop() while ($self->stacksize > 0
                         && (($self->tos)[2] != $right));
    if ($self->stacksize == 0) {
        $self->spush( [$self->left,$self->top,$self->right,$self->bottom] );
    }
    &util::debug('layout', "Clearright, next frame is " .
                 join(',',$self->tos));
    return 1;
}

sub clearboth {
    my $self = shift;
    my $thresh = $self->thresh;
    my $right = $self->right;
    my $left = $self->left;

    $self->spop() while ($self->stacksize > 0
                         && ($self->tos)[2] != $right
                         && ($self->tos)[0] != $left);
    if ($self->stacksize == 0) {
        $self->spush( [$self->left,$self->top,$self->right,$self->bottom] );
    }
    &util::debug('layout', "Clearboth, next frame is " .
                 join(',',$self->tos));
    return (abs(($self->tos)[2] - $right) <= $thresh
            && abs(($self->tos)[0] - $left) <= $thresh);
}

sub print_stack {
    my $self = shift;
    my $s = '';
    my $st = $self->{'stack'};
    local($,) = ",";
    for (@$st) {
        $s .= "[" . join(",", @{$_}) . "] ";
        #print STDERR "[", @{$_}, "]\n";
    }
    return $s;
}


=back

=head1 IMPLEMENTATION

The packer implements the following basic algorithm.  The state consists of
a I<stack> S, a I<current frame> CF, and a I<push frame> PF.  CF
represents the rectangular subregion of the "full page" that is
currently available for packing.  PF represents (approximately) the
region in which packing will continue after CF's area is exhausted.   An
invariant is that CF is always at
the top of the stack, followed by PF if there is one.

Initially, S contains two copies of the frame corresponding to the full
screen.

To pack an object O whose dimensions can be represented by the frame
OF:

=over 4

=item 1.

If the object will not fit in CF, return -1 immediately.

=item 2.

Pack the object against the left or right of CF, as specified.  For
centering, extend the object to full width of CF.

=item 3.

If the object is not as wide as CF, partition up the space and set CF
and PF as shown in the before-and-after figure below.  (Recall that CF
is always made the topmost stack element, followed by PF.)

 +---------------+     +-------+-------+
 |     CF        |     |   O   |new CF |
 |               |     +-------+-------+
 |               |     |     new PF    |
 |               |     |               | 

=item 4.

If the object is as wide as CF, there are three cases, depending on
whether the bottom of the object's frame is above, below, or in line
with the top of PF.  In all of the following diagrams, "P" represents an
area already packed (filled), PF represents the push frame,  CF the
current frame, and O the object being packed.

=item 4a.

Case (a): Bottom of OF is above top of PF.  Corresponds to the
before-and-after figures
below: we have not used up all of the packing area available in CF.  In
this case, leave PF unchanged, and set CF to the new smaller frame.

 +------+--------+        +------+--------+
 |  P   | old CF |        |  P   |   O    |
 |      |        |        |      +--------+
 +------+        |        +------+ new CF |
 | PF   |        |        | PF   +--------+
 |      |        |        |      |        |

=item 4b.

Case (b): Bottom of OF is below top of PF.  Packing should continue in
the area adjoining PF, rather than in CF.  So we interchange CF and PF
on the stack.

 +------+--------+        +------+--------+
 |  P   | old CF |        |  P   |   O    |
 |      |        |        |      |        |
 +------+        |        +------+        |
 | PF   |        |        | new  +--------+
 |      |        |        | CF   | new PF |
 |      |        |        |      |        |

=item 4c.

Case (c): Bottom of OF is exactly in line with top of PF.  In this case
we discard the old PF, since we can merge it into the remaining
space in the CF.  The new PF will be whatever was on the stack below PF,
so that when the packing algorithm is run recursively, everything will
work.

 +------+--------+        +------+--------+
 |  P   | old CF |        |  P   |   O    |
 |      |        |        |      |        |
 +------+        |        +------+--------+
 | PF   |        |        |    new        |
 |      |        |        |     CF        |
 |      |        |        |               |

=back

Packing of flowable objects such as text is handled by calling a
measurement routine 
to determine how much of the object will fit in a "static" chunk that
fills the width of CF, and continuing to pack until there is no more of
the flowable object left.

Clear is implemented by popping a single frame.
Clearleft, clearright, and clearboth are implemented by popping stack
frames until CF's left, right, or both edges correspond to the actual
screen bounds.

=head1 COPYRIGHT

Copyright (C) 1994-1996 by the Regents of the University of California.
This software is distributed under the terms of the GNU General Public
License, version 1.0 or greater.

=cut    

1;
