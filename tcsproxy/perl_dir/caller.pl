use clib;
use TACCutils;

sub DistillerInit {
    unless (&InitializeDistillerCache) {
        warn  "Distiller cache init failed";
        return 8;
    }
    return 0;
}

sub DistillerMain {
    my($data,$hdrs,$type,%args) = @_;

    $data = &insert_at_top($data,"<h1>Your page in all uppercase...how do ya like THEM apples?</h1><hr>");

    my($status,$newdata,$newhdrs) = &Distill("text/html",
                                             "fox/test/callee",
                                             $hdrs,
                                             $data);
    if ($status != 0) {
        warn "Status was $status\n";
    }

    warn "Status is now $status\n";
    return($newdata,$newhdrs,"text/html",$status);
}

1;


    
