#  -*- perl -*-
#
#  Simple font metrics for Pilot
#

package PilotFonts;
require Exporter;
require SimpleFont;

@ISA = qw(Exporter SimpleFont);

%fontID = ('stdFont' =>  0x2328,
           'boldFont' => 0x2329,
           'largeFont' => 0x232a,
           'symbolFont' => 0x232b,
           'symbol11Font' => 0x232c,
           'symbol7Font' => 0x232d,
           'ledFont' => 0x232e,
           );
              
@EXPORT = (keys (%fontID));

sub init_fonts {
    my $fontDir = shift;

    die "Cannot locate Pilot fonts in directory '$fontDir'\n"
        unless -d $fontDir && -r $fontDir;

    foreach $fnt (@EXPORT) {
        my ($buf,$i);
        my @wx;
        for (0..255) { undef $wx[$_]};
        $buf = '';
        open(F, sprintf("$fontDir/NFNT%04x.bin", $fontID{$fnt}))
            or die $!;
        while(<F>) { $buf .= $_ };
        close F;

        #  font header
        my ($type, $first, $last, $maxwidth, $kernmax, $descent, 
            $fwidth, $fheight, $offset, $ascender, $descender,
            $leading, $rowwords) = unpack("n13", $buf);
        $buf = substr($buf, 26);

        $last++;   

        # skip font data
        $buf = substr($buf, $rowwords*2*$fheight);

        # offset table
        for $i ($first..$last+1) {
            $pos[$i] = unpack("n", $buf);
            $buf = substr($buf, 2);
            #  pint "$i $pos[$i]\n";
        }

        # width table
        for $i ($first..$last+1) {
            ($offset[$i], $wid) = unpack("cc", $buf);
            $wx[$i] = $wid unless $wid==-1;
            $buf = substr($buf, 2);
            #  print "$i $offset[$i] $width[$i]\n";
        }

        # set remaining width table entries for ISO-8859-1 to width of slug
        for (0..255) {
            $wx[$_] = $wx[$last+1] unless defined $wx[$_];
        }
        $#wx = 255;
        $$fnt = new SimpleFont ($type, $first, $last, $maxwidth, $kernmax, $descent, 
                                $fwidth, $fheight, $offset, $ascender, $descender,
                                $leading, $rowwords, \@wx);
    }
    1;
}

1;

