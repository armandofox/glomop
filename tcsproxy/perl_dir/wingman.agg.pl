
use TACCutils;
use clib;
use CGI;

sub message {
    &MonitorClient_Send("Wingman Aggregator Messages", shift @_, "Log");
}


sub create_agg {
    my ($name, $descr, $help, $proc) = @_;

    $helplines .= "<LI><B>$name</B><UL><LI><EM>$descr</EM><LI>$help</UL>\n";

    ## Special case: the "agglist" aggregator doesn't show up
    ## in the agglist itself.
    if ($name !~ /^agglist/i) {
	$agglist .= $name."\0";
	++$aggcount;
    }

    $name =~ tr/A-Z/a-z/;
    $name =~ tr/a-z//dc;
    $aggprocs{$name} = $proc;
    &message("Creating an aggregator for type \'$name\'\n");
}


sub agg_redirect {
    my ($url) = @_;
    my $hdrs = <<EOH;
HTTP/1.0 301 Found\r
Location: $url\r
Content-type: text/html\r
\r
EOH
    my $body = <<EOB;
<HTML>
<HEAD>
<TITLE>HTTP Redirect</TITLE>
</HEAD>
<BODY>
<p>
This is an HTTP Redirect. Click <a href="$url">here</a>, 
if your browser does not automatically go to the redirected URL.
</BODY>
</HTML>
EOB

    ($body, $hdrs, "text/html", 0);
}


sub agg_agglist {
    my $hdrs = <<EOH;
HTTP/1.0 200 OK\r
Content-type: application/x-wingman-agglist\r
Pragma: no-cache\r
\r
EOH
    my $body = pack ('n', $aggcount).$agglist;
    ($body, $hdrs, 'application/x-wingman-agglist', 0);
}


sub agg_moreaggs {
    my ($name, $val, $argref) = @_;

    my $hdrs = <<EOH;
HTTP/1.0 200 OK\r
Content-type: application/x-wingman-agg\r
Pragma: no-cache\r
\r
EOH
    my $body = "agg://$name/$val";

    my ($a, $at, $an, @argarray);
    foreach $a (keys %$argref) {
        ($at, $an) = $a =~ /^(.)(.*)$/;
	push(@argarray, &make_argument($at, $an, $argref->{$a}));
    }

    if (defined $moreaggs_workers{$name}) {
	$worker = $moreaggs_workers{$name}
    } else {
	&message("Can't find worker for type \'$name\'\n");
	return ("",
		"HTTP/1.0 500 Fatal Error\r\nContent-type: text/html\r\n\r\n",
		"text/html", 8); # 8 => distFatalError
    }

    my ($retcode, $data);
    &message("Aggregator of type \'$name\' requires a "
	     . "'$moreaggs_workers{$name}' worker\n");
    ($retcode, $body, $hdrs) = &Distill('application/x-wingman-agg', 
					$moreaggs_workers{$name}, $hdrs,
					$body, @argarray);
    my $nhdrs = &make_headers($hdrs);
    my $type = $nhdrs->content_type;
    ($body,$hdrs,$type,$retcode);
}


sub agg_hotbot {
    ## Hotbot search

    ## <OPTION VALUE="MC"     >all the words
    ## <OPTION VALUE="SC"     >any of the words
    ## <OPTION VALUE="phrase" >exact phrase
    ## <OPTION VALUE="title"  >the page title
    ## <OPTION VALUE="name"   >the person
    ## <OPTION VALUE="url"    >links to this URL
    ## <OPTION VALUE="B"      >Boolean phrase

    my ($name, $val, $argref) = @_;
    &agg_redirect("http://www.hotbot.com/lite?SM=MC&MT=$val");
}

sub agg_altavista {
    my ($name, $val, $argref) = @_;
    &agg_redirect("http://www.altavista.digital.com/cgi-bin/query?pg=q&text=yes&what=web&kl=XX&q=$val");
}

sub agg_yahoo {
    my ($name, $val, $argref) = @_;
    &agg_redirect("http://search.yahoo.com/search?p=$val");
}

sub agg_stockquotes {
    my ($name, $val, $argref) = @_;
    &agg_redirect("http://quote.yahoo.com/q?symbols=$val&detailed=t");
}

sub agg_tripquest {
    my ($name, $val, $argref) = @_;
    my ($saddr, $scity, $sstate, $daddr, $dcity, $dstate, $mode)
	= split(/%0a/i, $val);
    $mode =~ tr/A-Z/a-z/;
    $mode = "overview" unless ($mode eq "turns" || $mode eq "text");
    ## Fetch the tripquest page
    &agg_redirect("http://www.mapquest.com/cgi-bin/mqtrip?" .
	"screen=tq_page&OPC=-1&ADDR_ORIGIN=" . ($saddr) .
	"&CITY_ORIGIN=" . ($scity) . "&STATE_ORIGIN=" .
	($sstate) . "&DPC=-1&ADDR_DESTINATION=" .
	($daddr) . "&CITY_DESTINATION=" . ($dcity) .
	"&STATE_DESTINATION=" . ($dstate) .
	"&quest_mode=trek&results_display_mode=" . $mode);
}

sub agg_dejanews {
    my ($name, $val, $argref) = @_;
    &agg_redirect("http://search.dejanews.com/bg.xp?level=$val&ST=BG");
}

sub agg_help {
    my ($name, $val, $argref) = @_;

    my $hdrs = <<EOH;
HTTP/1.0 200 OK\r
Content-type: text/html\r
Pragma: No-cache\r
\r
EOH

    my $body = <<EOB;
<HTML>
<HEAD>
<TITLE>
Help on Aggregators
</TITLE>
</HEAD>
<P>An <b>aggregator</b> is a service that visits one or more web sites,
gathers data you request, and returns it to you.  In <a
href="http://www.isaac.cs.berkeley.edu/pilot/wingman/about.html">Top
Gun Wingman</a>, aggregators are used to provide access to some common
net services that would otherwise require client-side form support.

<P>There are a number of aggregators currently supported.  To use one of them
from Wingman, enter its name into the <b>Aggregator name</b> field on
the Aggregator Control form.  Most aggregators can take commands; enter
the commands appropriate for the aggregator you want (described below) into the
<b>Aggregator commands</b> field.

<P><B>Check this page often, as we add aggregators all the time!</B>

<P>The currently suppored aggregators are:

<UL>
$helplines
</UL>
</HTML>
EOB

    ($body, $hdrs, 'text/html', 0);
}


sub DistillerInit {
    my $status = &InitializeDistillerCache;
    return $status if ($status != 1);

    $helplines = '';
    $agglist = '';
    $aggcount = 0;

    &create_agg('Hotbot', 'Use the Hotbot search engine',
		'In the commands field, enter the key words to search for.',
		\&agg_hotbot);
    &create_agg('AltaVista', 'Use the AltaVista seach engine',
                'In the commands field, enter the key words to search for.',
                \&agg_altavista);
    &create_agg('Search Yahoo', 'Search Yahoo',
                'In the commands field, enter the key words to search for.',
                \&agg_yahoo);
    &create_agg('Yahoo Stock Quotes', 'Get a stock quote from <a href="http://quote.yahoo.com/">quote.yahoo.com</a>',
                'Enter the symbols in the commands field',
                \&agg_stockquotes);
    &create_agg('Browse Deja News', 'Browse the Usenet hierarchy via Deja News',
                'In the commands field, enter the group name or hierarchy to view',
                \&agg_dejanews);
    &create_agg('TripQuest',
		"Get driving directions from MapQuest\'s TripQuest",
		"Enter the following in the commands field:<UL>" .
		"<LI>Line 1: Origin address<LI>Line 2: Origin city" .
		"<LI>Line 3: Origin state<LI>Line 4: Destination address" .
		"<LI>Line 5: Destination city<LI>Line 6: Destination state" .
		"<LI>Line 7: (Optional) one of <B>text</B>, <B>overview</B>," .
		" or <B>turns</B></UL>", \&agg_tripquest);
    &create_agg('Help', 'Get help', 'Enter nothing in the commands field', 
		\&agg_help);
    &create_agg('Agglist', 'Retrieve the aggregator list', 
		'Enter nothing in the commands field', 
		\&agg_agglist);

    # add the aggregators from the gm_options file
    my $moreaggs = &Options_Find("wingman.moreaggs") || '';
    my @aggs = split(' ', $moreaggs);
    my ($name, $descr, $help, $worker);
    $worker = '';
    foreach $name (@aggs) {
	$descr  = &Options_Find("wingman.agg.$name.descr")  || '';
	$help   = &Options_Find("wingman.agg.$name.help")   || '';
	$worker = &Options_Find("wingman.agg.$name.worker");
	if (length($worker) == 0) {
	    &message(<<"EOM");
Cannot find a worker for aggregator type \'$name\'
          Add an entry for wingman.agg.$name.worker to your gm_options file
EOM
            die;
	}

	$moreaggs_workers{$name} = $worker;
	&create_agg($name, $descr, $help, \&agg_moreaggs);
	&message("Adding a worker for type \'$name\': "
		 . "\'$moreaggs_workers{$name}\'  '$worker'\n");
    }
    
    0;
}



sub DistillerMain {
    my ($data,$hdrs,$type,%args) = @_;
    my ($proc);
    my ($name, $val) = $data =~ m!^agg://([^/]*)/?(.*)!;
    if (!defined $name) {
	$name = 'help';
	$val = 'error';
    }
    $name =~ tr/A-Z/a-z/;
    $name =~ tr/a-z//dc;
    unless (length($name) > 0) {
	$name = 'help';
	$val  = 'error';
    }

    $val = '' unless defined $val;

    if (defined $aggprocs{$name}) {
	$proc = $aggprocs{$name};
    } else {
	$proc = $aggprocs{'help'};
	$val = $name;
    }

    &$proc($name, $val, \%args);
}


__END__

use LWP;
use CGI;

$helplines = '';
$agglist = '';
$aggcount = 0;

sub help {
    $helplines .= "<LI><B>$_[0]</B><UL><LI><EM>$_[1]</EM><LI>$_[2]</UL>\n";
    $agglist .= $_[0]."\0";
    ++$aggcount;
}

sub proxypost {
    my ($loc, $data) = @_;
    my $ua = new LWP::UserAgent;
    my $req = new HTTP::Request("POST", $loc);
    $req->content_type('application/x-www-form-urlencoded');
    $req->content($data);
    my $response = $ua->request($req);
    $response->header(Location => $loc);
    $response->header(Content_Location => $loc);
    "HTTP/1.0 " . $response->code . " " . $response->message . "\n" .
	    $response->headers_as_string . "\n" . $response->content;
}

$query = new CGI;
$query->nph(1);
$p = $query->path_info();

($name, $val) = $p =~ m!^/(.+?)/(.*)!os;

$name = "help" if length($name) == 0;
$name =~ y/A-Z/a-z/;
$name =~ s/[^a-z]//g;

&help('Hotbot','Use the Hotbot search engine','In the commands field, enter the key words to search for.');
if ($name eq 'hotbot') {
    ## Hotbot search

    ## <OPTION VALUE="MC"     >all the words
    ## <OPTION VALUE="SC"     >any of the words
    ## <OPTION VALUE="phrase" >exact phrase
    ## <OPTION VALUE="title"  >the page title
    ## <OPTION VALUE="name"   >the person
    ## <OPTION VALUE="url"    >links to this URL
    ## <OPTION VALUE="B"      >Boolean phrase
    my $escval = CGI::escape($val);
    print $query->redirect("http://www.hotbot.com/lite?SM=MC&MT=$escval");
    exit;
}

&help('AltaVista','Use the Altavista search engine','In the commands field, enter the key words to search for.');
if ($name eq 'altavista') {
    ## Altavista search

    my $escval = CGI::escape($val);
    print $query->redirect("http://www.altavista.digital.com/cgi-bin/query?pg=q&text=yes&what=web&kl=XX&q=$escval");
    exit;
}

&help('Search Yahoo','Search Yahoo','In the commands field, enter the key words to search for.');
if ($name eq 'searchyahoo') {
    ## Yahoo search

    my $escval = CGI::escape($val);
    print $query->redirect("http://search.yahoo.com/search?p=$escval");
    exit;
}

&help('TripQuest',"Get driving directions from MapQuest\'s TripQuest","Enter the following in the commands field:<UL><LI>Line 1: Origin address<LI>Line 2: Origin city<LI>Line 3: Origin state<LI>Line 4: Destination address<LI>Line 5: Destination city<LI>Line 6: Destination state</UL>");
if ($name eq 'tripquest') {
    my ($saddr, $scity, $sstate, $daddr, $dcity, $dstate) = split(/\n/, $val);
    my ($mode) = "overview"; ## Choices: overview, turns, text
    ## Fetch the tripquest page
    print $query->redirect("http://www.mapquest.com/cgi-bin/mqtrip?" .
	"screen=tq_page&OPC=-1&ADDR_ORIGIN=" . CGI::escape($saddr) .
	"&CITY_ORIGIN=" . CGI::escape($scity) . "&STATE_ORIGIN=" .
	CGI::escape($sstate) . "&DPC=-1&ADDR_DESTINATION=" .
	CGI::escape($daddr) . "&CITY_DESTINATION=" . CGI::escape($dcity) .
	"&STATE_DESTINATION=" . CGI::escape($dstate) .
	"&quest_mode=trek&results_display_mode=" . $mode);
    exit;
}

&help('Yahoo Stock Quotes','Get a stock quote from <a href="http://quote.yahoo.com/">quote.yahoo.com</a>','Enter the symbols in the commands field');
if ($name eq 'yahoostockquotes') {
    my $escval = CGI::escape($val);
    print $query->redirect("http://quote.yahoo.com/q?symbols=$escval&detailed=t");
    exit;
}

&help('Browse Deja News','Browse the Usenet hierarchy via Deja News','In the commands field, enter the group name or hierarchy to view');
if ($name eq 'browsedejanews') {
    my $escval = CGI::escape($val);
    print $query->redirect("http://search.dejanews.com/bg.xp?level=$val&ST=BG");
    exit;
}

&help('Alpha page','Send a USA Mobile alpha page','Enter the following in the commands field:<UL><LI>Line 1: Your name<LI>Line 2: Pager Phone Number<LI>Other lines: your message</UL>');
if ($name eq 'alphapage') {
    my ($name,$number,$msg) = split(/\n/,$val,3);
    substr($msg, 160) = '' if length($msg) > 160;
    print &proxypost('http://www.usamobile.com/cgi-bin/email',
	"name=".CGI::escape($name)."&email=WEB+PAGER&recipient=".
	CGI::escape($number)."&content=".CGI::escape($msg));
    exit;
}

&help('SkyTel page','Send a SkyTel SkyWord page',"Enter the following in the commands field:<UL><LI>Line 1: Recipient's PIN<LI>Other lines: message</UL>");
if ($name eq 'skytelpage') {
    my ($number,$msg) = split(/\n/,$val,2);
    substr($msg, 240) = '' if length($msg) > 240;
    print $query->redirect("http://www.skytel.com/Paging/page.cgi?" .
	    "success_url=&to=".CGI::escape($number)."&pager=1&message=" .
	    CGI::escape($msg));
    exit;
}

&help('Help','Get help','Enter nothing in the commands field');

if ($name eq 'agglist') {
    my $newloc = $query->url();
    print $query->header('-Status'=>'302 Found',
                         '-Location'=>$newloc,
			 '-URI'=>$newloc,
			 '-nph'=>1,
			 '-type'=>'application/x-wingman-agglist',
			 '-Content-Length',2+length($agglist));
    print pack('n', $aggcount);
    print $agglist;
    exit;
}

## Help screen
print $query->header;
print $query->start_html(-title => 'Help on Aggregators');
if ($name ne "help") {
    print "<h2>Invalid aggregator specified</h2>\n<HR>\n";
}
print <<'EOH';
<P>An <b>aggregator</b> is a service that visits one or more web sites,
gathers data you request, and returns it to you.  In <a
href="http://www.isaac.cs.berkeley.edu/pilot/wingman/about.html">Top
Gun Wingman</a>, aggregators are used to provide access to some common
net services that would otherwise require client-side form support.

<P>There are a number of aggregators currently supported.  To use one of them
from Wingman, enter its name into the <b>Aggregator name</b> field on
the Aggregator Control form.  Most aggregators can take commands; enter
the commands appropriate for the aggregator you want (described below) into the
<b>Aggregator commands</b> field.

<P><B>Check this page often, as we add aggregators all the time!</B>

<P>The currently suppored aggregators are:

<UL>
EOH
print $helplines;
print <<'EOH';
</UL>
EOH
print $query->end_html;


sub DistillerInit {

    $helplines = '';
    $agglist = '';
    $aggcount = 0;


    %dispatch = ( 'agglist' => Agg,
                  '' => 'wingman.text',
		  'image/gif' => 'wingman.text',
		  'image/jpg' => 'wingman.text',
		  'image/jpeg' => 'wingman.text',
		  'application/zip' => 'wingman.zip',
		  'application/x-zip-compressed' => 'wingman.zip',
		  'application/x-wingman-agg' => 'wingman.agg' );

    0;
}


sub DistillerMain {
    
}
