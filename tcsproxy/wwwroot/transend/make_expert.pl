#!/usr/sww/bin/perl

$files = `find ../.. -name ARGS.h -print`;
$str = '<table border="1">';

sub string_add_arg {
    my($name,$id,$type) = @_;

    return <<EOS;
<tr><td>$name ($type)</td>
<td><input type="text" size="6" name="${type}${id}" transendid="${type}${id}">
</td>
</tr>
EOS
}

foreach $file (split (/\s+/,$files)) {
    open(FILE,$file) or die "$file: $!";
    while ($_ = <FILE>) {
        if ( m!define\s+(\S+)\s+(\S+)\s*/\*\s*(i|f|s)! ) {
            ($name,$id,$type) = ($1,$2,$3);
            if ($id =~ /\D/) {
                $id =~ s/([A-Za-z_]+)/\$$1/g;
                $id = eval $id;
            } else {
                eval "\$$name = $id";
            }
            $str .= &string_add_arg($name,$id,$type);
        }
    }
    close FILE;
}

$str .= "</table>\n";

$cmd = "$^X -i.bak -pe 's|<!--\\s*ARGUMENTS\\s*-->|$str|' expert.html";
system($cmd) and warn "Substitution failed ($?)";



