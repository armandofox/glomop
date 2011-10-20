# -*- perl -*-

require "util.pl";
        
package Para;
require Exporter;
@ISA = qw(Exporter);

&util::instvars('Para', qw(font otext flags text width height x y
                           link link_alt serv_x serv_y));

@EXPORT = qw($F_BOLD
             $F_ITALIC
             $F_UNDERLINE
             $F_SIZE0
             $F_SIZE1
             $F_SIZE2
             $F_SIZE3
             $F_IMAGE
	     $F_IMAGE_2BIT
	     $F_IMAGE_HAS_SERV_SIZE
	     $F_IMAGE_ISMAP
             $F_LINK
             $F_LINE
             $F_LINK_ALT
             $F_NEWLINE
             );

# font attributes
$F_BOLD =       0x0001;
$F_ITALIC =     0x0002;
$F_UNDERLINE =  0x0004;
$F_SIZE0 =      0x0000;
$F_SIZE1 =      0x0008;
$F_SIZE2 =      0x0010;
$F_SIZE3 =      0x0018;
$F_IMAGE =      0x0020;
$F_IMAGE_2BIT = 0x0001;
$F_IMAGE_HAS_SERV_SIZE = 0x0002;
$F_IMAGE_ISMAP = 0x0004;
$F_LINK  =      0x0040;
$F_LINE  =      0x0080;
$F_LINK_ALT =   0x0100;
$F_NEWLINE =    0x8000;

sub numlines {
    my $para = shift;
    my $t= $para->text;
    return scalar grep(/\n/s, $t);
}

sub flags_set {
    my($self,$newflag) = @_;
    return ($self->{'flags'} |= 0+$newflag);
}

sub flags_clear {
    my($self,$newflag) = @_;
    return ($self->{'flags'} &= ~(0+$newflag));
}    

sub new {
    my($class,$fontmetrix) = @_;
    bless  {
        "font" => $fontmetrix,
        "otext" => '',
        "flags" => 0,
        "text" => '',
        "width" => 0,
        "height" => 0,
        "x" => 0,
        "y" => 0,
        "link" => undef,
        "link_alt" => undef,
	"serv_x" => 0,
	"serv_y" => 0,
    }, $class;
}

sub clone {
    my %newself = %{shift @_};
    bless(\%newself, 'Para');
    return \%newself;
}

sub len {
    return length(shift->otext);
}

sub lastlinewidth {
    my $self = shift;
    my $lastline = $self->text;
    $lastline =~ m/[^\n]*$/;
    return length($&);
}

sub add {
    my ($self,$str) = @_;
    $self->{'otext'} .= $str;
}

sub close {
    my ($self) = shift;
    #my @lines = $self->split_string;
    #return ($self->{'text'} = join("\n", @lines));
}

