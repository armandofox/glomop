#  Hey emacs, this is a -*- perl -*- program
#  A trivial distiller that converts keywords identified by arg id 9000 to be
#  red.
#

use TACCutils;

sub DistillerInit {
    0;                                      # the distOk return code
}

sub DistillerMain {
    my($data,$headers,$type,%args) = @_;

    # regexp to highlight

    $pattern = "research";

    $data = &html_regsub($data,
                         "s!($pattern)!<font size=+2 color=red><b>\$1</b></font>!ig");

    $data =~ y/a-z/A-Z/;
    # since this is a simple distiller, return an empty string and let the
    # frontend construct the headers.
    $hdrs = "HTTP/1.0 200 OK\r\nX-Route: transend/text/html\r\n\r\n";
    return($data, $hdrs, "text/html", 10);
}


1;

