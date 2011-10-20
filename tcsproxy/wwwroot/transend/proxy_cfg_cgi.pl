#!/usr/sww/bin/perl
@all_frontends = qw(transend1.cs.berkeley.edu:4444
                    transend2.cs.berkeley.edu:4444
                    transend3.cs.berkeley.edu:4444
                    transend4.cs.berkeley.edu:4444
);

$num_frontends = scalar @all_frontends;
$num_bits = int(.5 + log($num_frontends)/log(2));
($which_fe,@crud) = unpack("%${num_bits}C*", $ENV{'REMOTE_ADDR'});
$which_fe %= $num_frontends;

# reorder the list with that one in front
@all_frontends = (@all_frontends[$which_fe..$num_frontends-1],
                  @all_frontends[0..$which_fe-1])
    if $which_fe > 0;
$proxy_list = join('; ', grep(s/^/PROXY /, @all_frontends)) . "; DIRECT";

print <<EOM;
Content-type: application/x-ns-proxy-autoconfig

function FindProxyForURL(url, host)
{
     if (url.substring(0, 5) == "http:") {
         return "$proxy_list";
     } else {
         return "DIRECT";
     }
}

EOM
;    
