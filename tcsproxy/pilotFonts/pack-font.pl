#!/usr/bin/perl

$lines = 0;
$min = 257;
$max = -1;
$offset = 0;

while(<>) {
  chop;
  s/\s*#.*$//;
  if (/^Type\s+(\S+)/i) {
    if ($1 =~ /^0/) {
      $type = oct($1);
    } else {
      $type = $1+0;
    }
  } elsif (/^Height\s+(\d+)/) {
    $height = $1;
  } elsif (/^A\S*\s*(\d+)/i) {
    $ascender = $1;
  } elsif (/^Descend\S*\s*(\d+)/i) {
    $descender = $1;
  } elsif (/^Descent\S*\s*(\d+)/i) {
    $descent = $1;
  } elsif (/^L\S*\s*(\d+)/i) {
    $leading = $1;
  } elsif (/^K\S*\s*(\d+)/i) {
    $kernmax = $1;
  } elsif (/^Char/i) {
    goto dochar;
  } elsif (/^\s*$/) {
  } else {
    print "Unknown command '$_' at line $. of $ARGV\n";
  }
}

sub dochar {
  if ($height == 0) {
    $height = $lines;
  }
  if ($lines != $height) {
    print "Warning: irregular data height at line $. of $ARGV\n";
  }
  
  $width[$char] = $width;
  $offset[$char] = $offset;
  $lines[$char] = $l;
  $lines[$char] =~ tr/X./10/;
  
  if ($char<$min) {
    $min = $char;
  }
  if (($char<256) && ($char>$max)) {
    $max = $char;
  }
  
  $maxwidth = $width if $width>$maxwidth;
  
  $l = "";
  $lines = 0;
  $width = 0;
  $offset = 0;
  
}

while(<>) {
  chop;
  s/'#'/35/;
  s/\s*#.*$//;
  if (/^char/i) {
    dochar();
dochar:
    $lines = 0;
    if (/^char\s*'(.)'/i) {
      $char = ord($1);
    } elsif (/^char\s*(\d+)/i) {
      $char = $1;
    } elsif (/^char\s*slug/i) {
      $char = 256;
    }
  } elsif (/^offset\s*(\d+)/) {
    $offset = $1;
  } elsif (/^([\.X]+)\s*$/) {
    if ($width == 0) {
      $width = length($1);
    } elsif (length($1) != $width) {
      print "Warning: irregular data length at line $. of $ARGV\n";
    }
    $l.=$1;
    $lines++;
  } elsif (/^\s*$/) {
  } else {
    print "Unknown command '$_' at line $. of $ARGV\n";
  }
}

if ($lines) {
  dochar();
}

if ($min==257) {
  print "Error: the font must contain at least one character and a slug\n";
  exit;
}

if ($width[256]) {
  $width[$max+1] = $width[256];
  $offset[$max+1] = $offset[256];
  $lines[$max+1] = $lines[256];
} else {
  print "Error: the font must contain a slug\n";
}

for $y (0..$height-1) {
  $l = "";
  for $i ($min..$max+1) {
    $l .= substr($lines[$i], $y*$width[$i], $width[$i]);
  }
  $l .= "0" while length($l)%16;
  $line[$y] = $l;
}

$rowwords = length($l)/16;

$offset = ($rowwords*$height+($max+2-$min+6));

print pack("n13", $type, $min, $max, $maxwidth, $kernmax, $descent,
  $maxwidth, $height, $offset, $ascender, $descender, $leading, $rowwords);

for $y (0..$height-1) {
  print pack("B*", $line[$y]);
}

$pos = 0;
for $i ($min..$max+2) {
#  print "$i $pos\n";
  print pack("n", $pos);
  $pos += $width[$i];
}

for $i ($min..$max+2) {
  if (!$width[$i]) {
     print pack("cc", -1, -1);
#    print "$i -1 -1\n";
  } else {
     print pack("cc", $offset[$i], $width[$i]);
#    print "$i $offset[$i] $width[$i]\n";
  }
}

