package TACCutils;
require Exporter;
@ISA = qw(Exporter);

=head1 NAME

TACCutils - utilities for building TACC services

=head1 SYNOPSIS

       use TACCutils;

=head2 Tag-level HTML Munging

       ($what,$thing,$rest) = &next_entity($html);       
       $tag_text = &tag_from_attribs($tagname, $attribs_hash_ref);
       $eat = &zap_to_tag($string,$substr);

       $is_open = &tag_matches_open($tag,@list);
       $is_close = &tag_matches_close($tag,@list);
       $is_tag = &tag_matches($tag,@list);

       $magified_url = &tomagic($orig_url, $args);
       $full_path_url = &canonicalize($short_url, $base_url);

=head2 High-level HTML Munging

       %dates = &match_all_dates($text[,$date_comes_first_p]);
       $new_html = &html_regsub($html, $subst_expr);
       $html = &insert_at_top($old_html, $string);
       $html = &htmlify($text [,$title [,$body_options]]);
       $text = &striphtml($text[,$keep_comments_p[,$keep_tags_p[,$keep_entities_p]]]);

=head2 Forms Support

       %query_params = &make_cgi_args($headers);
       $string = &unescape($form_submission_string);

       $cgi_form = &make_cgi_form(@args);
       $prefs_form = &make_prefs_form(@args);

=head2 Manipulating and Examining Headers

       $my_url = &get_my_url($headers);

       $new_headers = &add_header($old_headers, $new_header, $new_value);       
       $request_succeeded = &status_ok($headers);

       $hdrs_object = &make_headers($orig_hdrs_as_string);
       $hdrs_object = new TACCutils::Headers($status_line, $hdrs_as_string);
       $status = $hdrs_object->status([$new_status]);
       $hdr_value= $hdrs_object->header("Content-length"[,$new_value]);

       $hdrs_as_text = $hdrs_object->as_string();

=head2 Miscellaneous

       %list = &qxurl($text);
       ($x,$y) = &gif_size($data);
       ($x,$y) = &jpeg_size($data);

=head1 DESCRIPTION

Here's a set of functions to make it easier to write aggregators.  You
should generally put the following incantation at the beginning of your
aggregator:

      use TACCutils;
      use clib;           #  access to cache and GUI monitor
      use Date::Manip;

=cut


use LWP;
use_alarm LWP::UserAgent (0);
#use Date::Manip;
require 5.002;      # for nifty embedded regexp comments

@EXPORT = qw(
             insert_at_top
             add_header
             entity
             next_entity
             tag_from_attribs
             tag_matches_open
             tag_matches_close
             tag_matches
             zap_to_tag
             tomagic
             canonicalize
             make_headers
             status_ok
             make_cgi_args
             unescape
             qxurl
             match_all_dates
             get_my_url
             make_prefs_form
             html_regsub
             htmlify
             jpeg_size
             gif_size
             );


# tag scanning state machine:
# state Initial: consume whitespace, tag name, optionally more whitespace
#    on ">" goto End
#    on other non-whitespace goto Attrib
# state Attrib: consume attrib name, optional whitespace
#    on "=" goto Value
#    on other non-whitespace goto Attrib
# state Value:
#    collect whitespace + (quotedstring OR  nonquote+nonquotedstring)
#        
# Possible return values:
#  (0,$markup,@comments)
#  (1,$tag,%attribs)
#  (2,$text)

=head2 Tag-Level HTML Munging

=over 3

=item  ($what,$thing,$rest) = &next_entity($html);

Returns the next "thing" in $html, which may be either a bunch of text
with no markup, a tag, or markup/comments.  Also removes that thing from
the front of $html (i.e. $html is modified in place), so you can call it
repeatedly (until $html is empty) to scan HTML.

If $what is 0, then $thing is a complete tag of SGML markup (i.e. it
begins with "<!").  If the markup contained SGML comments (delimited by
"--"), $rest is a reference to a linear array each element of which is one
comment; otherwise $rest is the empty string. 

If $what is 1, then $thing is an HTML tag I<name>, and $rest is a reference to 
an associative array of the tag attributes.  The attribute names (but not
the values) are
canonicalized to all lowercase.  Attributes with no value are defined as
keys with undef as their value.  So, for example, if $html began with
the following:

    <IMG SRC="foo.gif" ISMAP ALIGN=LEFT>

then $thing would be "img" and the attributes array would contain the
following key => value pairs:

    src  =>  foo.gif
    ismap => undef
    align => LEFT
    
(Note that any quotes surrounding attribute values are automatically removed.)
The special attribute "_all_" contains the entire tag text, verbatim from the
original source.

Finally, if $what is 2, then $thing is a bunch of text containing no
markup or tags, and $rest is empty.
$thing is not necessarily guaranteed to be the maximal
extent of such text, i.e. it is not necessarily true that the next call
to this function will return a markup or tag.

The implementation of this function is a simple state machine with
regular expressions controlling state transitions, so it is not as fast
as a regexp-only solution (though it seems to be more correct) but much
faster than a full-blown scanner/lexer.

As an invariant, this function always ensures that the modified-in-place value
of $data is strictly shorter before returning, so calling this function enough
times will eventually shrink $data to the empty string.

=cut
        
# to save some time, declare some variables global.
$g_in_comment = 0;    
$g_m = 0;
$g_tag = '';
%g_attribs = ();
$g_all = '';
$g_comment = '';
@g_comment = ();

sub next_entity {
    $_ = $_[0];                             # note, this is a REFERENCE!  we
                                            # modify $_[0] in place.
    if (/^<\s*!/) {                            # markup: consume and discard
        $g_in_comment = 0;
        $g_m = '';                            # result
        ($g_comment,@g_comment) = ();       # comments
        while (1) {
            if (!$g_in_comment) {
                # scan till single or double quotes, end of markup, or comment
                s/^[^->\"\']*//s;           #'" 
                $g_m .= $&;
                # now figure out what to do next
                $g_m .= $&,last if s/^>//;    # end of markup = done
                $g_in_comment = 1, $g_m .= '--', next if s/^--//; # start comment
                $g_m .= '-', next if s/^-//;  # illegal, but appears on some pages
                $g_m .= $&, next
                    if s/^([\"\'])[^\1]*?\1//s; # '"else consume quoted string
                $g_m .= $&;
            } else {
                # BUG::to parse sgml-compliant comments, the code below
                # really should have been:
                #  $g_comment .= $& while s/^[^-]+|^-[^-]//s;
                #  ...
                #  s/^--//;
                #  However, that fails because some cretin decided that both
                # "--" and ">" are legal javascript operators, so it's possible
                # to see the following string while ostensibly inside a
                # comment, which would close the comment in sgml:
                #   if ( a-- == foo) { ....
                # or even worse:
                #   if ( a--> foo) { ...
                # so instead we use a regexp that collects "netscape javascript
                # broken" style comments, which are presumed to always end
                #  in "-->".
                #  One way around this is for the caller of this function to
                # ignore everything until "/script" if "<script>" is seen.
                # collect comment
                $g_comment .= $&, $g_m .= $& while s/^[^-]+|^-(?!-\s*>)//s;
                push(@g_comment,$g_comment);
                $g_comment = '';
                $g_m .= $& if s/^--\s*>//;
                $g_in_comment = 0;
                last;
            }
        }
        $_[0] = $_;
        return (0,$g_m,\@g_comment);
    } elsif (/^</) {                        # looking at tag: return it
        # scan tag:
        %g_attribs = ();
        s/^<\s*([^>\s]+)\s*//s;                 # consume tag name, whitespace
        $g_all = length($&);
        $g_tag = lc $1;
        $g_all += length($&), ($g_attribs{lc $1} = $3) =~ s!^([\"\'])(.*)\1$!$2!g
            while s{
                (^[^>\s=]+)             # attrib name
                    (\s*=\s*            # 
                     ((\"[^\"]*\")      # quoted string...
		      |(\'[^\']*\')
                      |([^\'\">\s]*)))?   # or unquoted...both optional
                          \s*}{}sx ;   # trailing whitespace "
        s/^>//s;
        $g_all++;                             # length of '>'
        $g_attribs{'_all_'} = substr($_[0],0,$g_all);
        s/^.// if length($_[0]) == length($_);
        $_[0] = $_;
        return (1,$g_tag,\%g_attribs);
    } else {
        # snarf all text up to but not including start of next tag
        s/[^<]*(?=<)//s
            or s/[^<]*$//s;                 # catch untagged text at end of page
        $_[0] = $_;
        return (2,$&);
    }
}
        
=item $tag = &tag_from_attribs($tagname,$attribs)

Creates a string containing a complete tag, given a tagname and a reference to
a hash of
attributes (as returned,e.g., by L<next_entity>).

=cut

sub tag_from_attribs {
    my ($tagname,$attribs) = @_;
    my %attribs = %$attribs;
    my @l = ("<" . uc $tagname);
    my ($k,$v);

    while (($k,$v) = each %attribs) {
        next if $k =~ /^_[^_]*_$/;
        push(@l, join('=',uc($k),"\"$v\""));
    }
    return (join(' ', @l) . ">");
}

=item $eat = &zap_to_tag($html,$string);

Gobbles and returns all of the HTML markup and text, verbatim, up to and
including $string.  $html is modified in place, and is also returned
as the result of the call.
You can use this, e.g., if you see "<SCRIPT>" and
want to skip ahead until the corresponding "</SCRIPT>".  Note that no
actual parsing is done, so if you saw "<ul>" and tried to skip ahead to
"</ul>", you wouldn't get the desired result if lists were nested.
Similarly, if $string happens to appear inside a comment or some other
place where it should logically be ignored, this function won't do what
you want.

=cut


sub zap_to_tag {
    $_ = shift;
    my $str = shift;

    s!^.*?$str!!;
    $_[0] = $_;
    return $&;
}

=item &tag_matches_open($tag,@list), &tag_matches_close($tag,@list), &tag_matches($tag,@list)

Return a true value iff $tag matches one of the tag names in @list.
Matching is case insensitive.  C<&tag_matches_open("li", ("li"))> is true,
C<&tag_matches_open("/li", ("li"))> is false, C<&tag_matches_close("li","li")>
is false, and C<&tag_matches_close("/li", "li")> is true.  C<&tag_matches>
does not care if it is an open or close.

=cut

sub tag_matches_open {
    my ($tag,@list)=@_;
    for (@list) {
        return $_ if m!^\s*$tag$!i;
    }
    return undef;
}

sub tag_matches_close {
    my ($tag,@list) = @_;
    if ($tag =~ s!^/!!) {
        for (@list) {
            return $_ if  m!^\s*$tag$!i;
        }
    }
    return undef;
}

sub tag_matches {
    return (&tag_matches_open(@_) || &tag_matches_close(@_));
}

=item  $magified_url = &tomagic($orig_url, $args)

Given a URL and some magic to insert, returns the magified URL.  The
"magic" is actually argument/value pairs that can be inserted into the
URL so that when the URL is submitted as a request, these argument
values will override the user's default prefs.  Currently the usage is
ugly: $args must have the form "i5=26" (e.g.) for a single argument, or
the form "i5=26^s11=foo^f1=0.0" (e.g.) for multiple arguments.  

The magified URL is returned.

In the future this function will be intelligent about not duplicating
argument keys within a URL and its calling convention will not have the
"^" formatting hardwired.

=cut

sub tomagic {
    my $src = shift;
    my $magic =  shift;

    if ($src =~ /^(.*?\*\^.*\^)(\?.*)?$/) {
        $src = $1 . "$magic^$2";
    } elsif ($src =~ /^(.*?)(\?.*)$/) {
        $src = $1 . "*^$magic^$2";
    } else {
        $src .= "*^$magic^";
    }

    $src;
}

=item  $full_url = &canonicalize($short_url, $base_url);

Canonicalize $short_url relative to the base $base_url.  Use this to
fully qualify embedded URL's (which are often relative to the page
embedding them) before fetching them from the cache.  For example, if
$base_url is T<http://www.foo.com/frontpage.html>, and you canonicalize
the relative URL T<foobar.gif>, you will get the full URL
T<http://www.foo.com/foobar.gif>.

(This is just a wrapper around the more cumbersome functions in LWP.)

=cut

sub canonicalize
{
    my ($short,$base) = @_;
    ## The ,1 means to allow relative URLs that look like "http:/path/name"
    ## RFC1808 says that this should be avoided, but some people do it anyway.
    return (new URI::URL $short)->abs($base,1)->as_string;
}

#
#  High-level HTML munging
#

$day = '(sun?|sunday|mon?|monday|tue?s?|tuesday|wed?s?|wednesday|th?u?r?s?|thursday|fri?|friday|sat?|saturday)'; # 1
$sep = '[ \t,]+';                            # 0
$num = '(\d\d?)(rd|nd|st)?';                 # 2
$yr = '(19|20)?\d\d';                       # 1
$month = '(jan|january|feb|february|mar|march|apr|april|may|june?|july?|aug|august|sept?|september|oct|october|nov|november|dec|december)'; # 1

%dates = ("$day$sep$month$sep$num$sep$yr(?!\d)" => '"$2 $3"',
          "$day$sep$month$sep$num(?!\d)"  => '"$2 $3"',
          "$month$sep$num$sep$yr(?!\d)" => '"$1 $2"',
          "$month$sep$num(?!\d)" => '"$1 $2"',
          "$num$sep$month$sep$yr(?!\d)" => '"$3 $1"',
          "$num$sep$month" => '"$3 $1"',
          "[^/\d](\d\d?)/(\d\d?)/(\d\d\d?\d?)(?!\d)", '"$1/$2"',
          "[^/\d](\d\d?)/(\d\d?)(?!\d)", '"$1/$2"'
          );

=back

=head2 High-Level HTML Munging

=over 3

=item %dates = &match_all_dates($text [,$date_comes_first_p]);

Given HTML or plain text $text (works better with plain text), find
everything that appears to be a date,  parse it into standard form
"YYYYMMDD:HH:MM", and enter it as a key in a hash whose corresponding
value is all the text seen since the last date matched.  This has the
effect of returning a hash whose keys are dates and whose values are
(ostensibly) events happening on those dates.  If $date_comes_first_p is
true, value of each date is the text following it (up to the next date)
rather than preceding it.

=cut

sub match_all_dates {
    my($text) = shift;
    my($date_comes_first) = shift;
    my(%result) = ();

    my ($match,$x,$y,$k,$savex,$savetext);
    my ($curindex,$index);
    
    # severe optimization hackery:
    # build up a series of constant-regexp comparisons.

    my $eval = '';

    foreach $k (keys (%dates)) {
        $eval .= "\$match = q%$k%, \$curindex = length(\$`) ";
        $eval .= "if (m\`$k\`im  && length(\$`) < \$curindex);"
    }


    #warn "Match exp:\n\n$eval\n\n";
  FIND_DATES:
    while (1) {
        undef $match;
        $_ = $text;
        $curindex = 1e9;
        # try all regexps, pick the leftmost match

        eval $eval;
        #warn "ERROR:  $@\n\n" if $@;
        
        last FIND_DATES unless defined $match; # no more matches
        $text =~ m`$match`im;               # restore $1...$9
        $y = eval($dates{$match});
        #warn "Will use: $& ==>  `$y'\n";
        $x = &ParseDate($y);
        # grab all text from last matched date string till here
        $match = substr($text,0,length($`)-1);
        # delete all text up to and including end of this match
        $text = substr($text,length($`)+length($&));
        if ($date_comes_first) {
            $result{$savex} = $savematch if defined $savex;
            $savex = $x;
            $savematch = $match;
        } else  {
            $result{$x} = $match;
        }
    }
    return %result;
}

=item $html = &html_regsub($html, $subst)

Do regular expression substitution on HTML, in such a way that the tag contents
are not affected.  Mostly works.  $subst should be an actual substitution
expression that would work on the text if it were plain ASCII, for example,
's/$word/<B>\1</b>/gim'.  $subst will be eval'd after the HTML has been made
"safe".   Returns the new HTML.

=cut

sub html_regsub {
    my ($html,$subst) = @_;
    my $flagch = chr(0164);
    my ($tmphtml,%tags) = &striphtml($html,1,0,1,$flagch);
    
    eval "\$tmphtml =~ $subst";

    $tmphtml =~ s/$flagch(\d+)/"$tags{$1}"/ge;
    return $tmphtml;
}

    
=item $html = &insert_at_top($old_html, $string);

Given a chunk of HTML, insert $string at the "top" of the page.  Tries
various heuristics (checking for presence/absence of various tags) to
figure out where this is, and returns new HTML.

=cut

#
#  insert something at the "top" of an HTML page (after <BODY>, or after
#  <HTML>, or after </TITLE>, or just at beginning; whichever succeeds first)
#
sub insert_at_top {
    my ($html,$thing) = @_;
    $html =~ s!<\s*body[^>]*>!$&$thing!im
        || s!<\s*html[^>]*>!$&$thing!im
            || s!<\s*/title[^>]*>!$&$thing!im
                || s!^!$thing!im
                    ;
    return $html;
}


=item $html = &htmlify($text[,$title[,$bodyopts]]);

Wraps up $text in a nice HTML wrapper by adding HEAD, BODY, etc.  If 
no $title is given, a default one is used. $bodyopts can be anything
that would go in the BODY tag, e.g. 'BGCOLOR="#ffffff" VLINK="red"'.

=cut

sub htmlify {
    my $text = shift;
    my $title = shift || "HTML Output";
    my $bodyopts = shift;

    return qq!<HTML>\n<HEAD>\n<TITLE>$title</TITLE>\n</HEAD>\n<BODY $bodyopts>\n$text\n</BODY>\n</HTML>\n!;
}

=item $text = &striphtml($text[,$keep_comments_p[,$keep_tags_p[,$keep_entities_p]]]);

Strips HTML tags from $text, and makes it look sorta-kinda readable (by
converting <BR>'s into newlines, for example). The three optional
arguments indicate that comments, HTML tags, and "entities" (like
"&gt;")  are not to be touched.  It's not perfect, but it does get most
of the tags.

=cut

sub striphtml {
    my($k, $keep_comments_p, $keep_tags_p, $keep_entities_p,$save) = @_;
    $_ = $k;
#########################################################
# first we'll shoot all the <!-- comments -->
#########################################################

       if (!$keep_comments_p) {
s{ <!                   # comments begin with a `<!'
                        # followed by 0 or more comments;

    (.*?)		# this is actually to eat up comments in non 
			# random places

     (                  # not suppose to have any white space here

                        # just a quick start; 
      --                # each comment starts with a `--'
        .*?             # and includes all text up to and including
      --                # the *next* occurrence of `--'
        \s*             # and may have trailing while space
                        #   (albeit not leading white space XXX)
     )+                 # repetire ad libitum  XXX should be * not +
    (.*?)		# trailing non comment text
   >                    # up to a `>'
}{
    if ($1 || $3) {	# this silliness for embedded comments in tags
	"<!$1 $3>";
    } 
}gesx;                 # mutate into nada, nothing, and niente
}
#########################################################
# next we'll remove all the <tags>
#########################################################

    if (!$keep_tags_p) {
        my $i=0;
        if ($save) {
            s!<.*?>!{$i++; $tagz{"$i"}=$&; "$save$i"}!egs;
        } else {
            s!<.*?>!!gs;
        }
        #1 while s!<("[^"]*"|[^>'"]*|'[^']*')+>!!gs;


# s{ <                    # opening angle bracket

#     (                 # backreffing grouping paren
#          "[^"]*"          # a section between double quotes (stingy match)
#             |           #    or else
#          [^>'"] *       # 0 or more things that are neither > nor ' nor "
#             |           #    or else
#          '[^']*'          # a section between single quotes (stingy match)
#     ) +                 # repetire ad libitum
#                         #  hm.... are null tags <> legal? XXX
#    >                    # closing angle bracket
# }{}gsx;                 # mutate into nada, nothing, and niente
}                                           # "
#########################################################
# finally we'll translate all &valid; HTML 2.0 entities
#########################################################
if (!$keep_entities_p) {
s{ (
        &              # an entity starts with a semicolon
        ( 
	    \x23\d+    # and is either a pound (#) and numbers
	     |	       #   or else
	    \w+        # has alphanumunders up to a semi
	)         
        ;?             # a semi terminates AS DOES ANYTHING ELSE (XXX)
    )
} {

    $entity{$2}        # if it's a known entity use that
        ||             #   but otherwise
        $1             # leave what we'd found; NO WARNINGS (XXX)

}gex;                  # execute replacement -- that's code not a string
}

    # hack: if unclosed tag at end, delete it.
    s/<[^>]*$//s;

    if (wantarray) {
        return ($_, %tagz);
    } else {
        return $_;
    }
}



=back

=head2  Manipulating Headers

=over 4

=item $new_headers = &add_header($old_headers, $new_header, $new_value);       

Quick and dirty way to add a header to an existing set of headers.
Don't use this; use the "header object" stuff below, instead.

=cut

#
#  add a header (quick and dirty way)
#
sub add_header {
    my ($hdrs,$newhdr,$value) = @_;
    $newhdr = "$newhdr: $value" if $value;
    $newhdr .= "\r\n\r\n";
    $hdrs =~ s!\r?\n$!$newhdr!im;
    return $hdrs;
}

=item $request_succeeded = &status_ok($headers);

Returns true iff the given headers indicate that the status of the HTTP
response was 200 (success).  Operates on either a string or a headers
object.

=cut

#
#  return true iff status of request was "200 OK"
#  operates on either a string or TACCutils::Headers object
#
sub status_ok {
    my $hdrs = shift;
    if (ref($hdrs) eq 'TACCUtils::Headers') {
        $hdrs = $hdrs->status();
    } 
    return ($hdrs =~ /^\S+\s+200\s+/);
}

=item $my_url = &get_my_url($hdrs);

Given either a headers object or string of headers, returns the URL
in the Location header or undef if not found.  Useful for getting
the URL with which you were called.

=cut

sub get_my_url {
    my $hdrs = shift;
    my $url = '';
    
    if (ref($hdrs) eq 'HTTP::Headers') {
        $url = $hdrs->header("Location");
    } else {
        if ($hdrs =~ m/\r?\n?location:\s*(\S+)/i) {
            $url = $1;
        } 
    }
    return new URI::URL ($url || "http://");
}

=back

=head2 Forms Support

=over 4

=item $cgi_object = &make_cgi_args($headers);

Given a headers object or a "raw" string of headers, use the URL in the
"Location:" header to return a hash where the keys are argument names 
and the values are argument values from a form submission.
Returns undef if there is no "Location" header (and hence no way to get the
URL), or if the URL does not appear to be a CGI submission URL.  

=cut

#
#  make a CGI-arguments thingy from metadata
#
sub make_cgi_args {
    my $hdrs = shift;
    my $url  = &get_my_url($hdrs) || return undef;
    return ((new URI::URL $url)->query_form);
}

=item $string = &unescape($string);

Unescape a string, such as the kind that comes embedded in a form submission
URL. 

=cut

sub unescape {
    $_ = shift @_;

    s/\+/ /g;
    s/%([0-9A-Fa-f][0-9A-Fa-f])/chr(hex($1))/eg;
    return $_;
}

=item $cgi_form = &make_cgi_form(@fields[,$args]);

=item $cgi_form = &make_prefs_form(@fields[,$args]);

Create the HTML markup for a CGI submission form.  This can be used to
generate a form that will later be passed back to this aggregator with
arguments on it.  The only difference between these two calls is that
make_cgi_form creates a form that is eventually passed back to this
worker, whereas make_prefs_form creates one that is passed to the
prefs-setting mechanism.  This is useful, e.g., if this worker wants the
user to set a preference that it understands.

The args list is parsed 4 elements at a time, as a sequence of 4-tuples
(note that Perl doesn't support nested lists, so a list of 4-tuples
really just means a list with 4N elements).  Each group of 4 elements
should be a field name (descriptive text for the user's benefit), field
ID (should be a TranSend-style ID, like "s9001", for make_prefs_form),
field type (right now only "text" is supported), default value.  If the
default value is undef (which is b<NOT> the same as the empty string!),
the existing (current) value of the field ID is used instead. This has
the nice effect that by default, the field value for a given arg ID
corresponds to the user's current value of that arg ID.  To make this
work, the last argument to either of these functions must be a
I<reference to the hash> of the user profile arguments, i.e. a reference
to the C<%args> passed to C<DistillerMain>.  For example:

          $form = &make_prefs_form(@list_of_4tuples, \%args);

where we have used the Perl C<\> operator to create a reference to the
hash C<%args>.  

If the last argument to this function is I<not> a hash reference, it's
assumed to be part of the list of 4-tuples that are field
descriptors. (Don't you love Perl-style polymorphism?)

The returned form includes everything from <FORM>...</FORM>, but you
still need to add some descriptive text in front and wrap it up using
C<htmlify>. 

=cut

sub make_prefs_form {
    my @args = @_;
    my %hash;
    if (ref($args[$#args]) eq 'HASH') {
        %hash = %{pop @args};
    }
    my $form;
    my($name,$argid,$type,$val);
    my $servername = &clib::Options_Find("frontend.prefsFormSubmitUrl");
    
    return undef unless (scalar(@args) % 4 == 0);
    $form = qq!<FORM ACTION="$servername" METHOD="GET">!;

    while (@args) {
        $name = shift @args;
        $argid = shift @args;
        $type = shift @args;
        $val = shift @args;
        
        if (!defined($val) && defined(%hash)) {
            $val = &unescape($hash{$argid});
        }

        $form .= <<EndForm;
<BR>
    $name:
    <INPUT TYPE="text" SIZE="80" NAME="$argid" VALUE="$val"
     TRANSENDID="$argid" TRANSENDVAL="$val">
EndForm
    }
    $form .= '<BR><INPUT TYPE="Submit">';
    $form .= "\n</FORM>";

    return $form;
}
    
   
                   

#
# qxurl - quick url extraction program
# tchrist@perl.com
#
# quick url extraction program.  reads from files, not urls, and 
# does not adjust relative urls.  see xurl for a better but 100x 
# slower solution.
# 
#############
sub qxurl {
    local( $/ )= '';
    $_ = shift @_;
    my %list = ();
    $list{$1} = $2
        while m{
            < \s* 
                A \s+ HREF \s* = \s* (["']) (.*?) \1 # '"
                                       \s* >
                                       ([^<]*)
                                   }gsix;
    return %list;
}

@TACCutils::Headers::ISA = qw(HTTP::Headers Exporter);
#
#  A subclass of HTTP::Headers
#


=item $hdrs_object = &make_headers($orig_hdrs_as_string);

Given a set of headers (including the initial HTTP status line), creates
a TACCutils::Headers object, which inherits methods from HTTP::Headers
in addition to having its own methods below.  You should use this
abstraction to deal with headers.

If $orig_hdrs_as_string is the empty string, an empty set of headers with the
status line  "HTTP/1.0 200 OK" will be returned.

=cut

#
#  make headers object from a string which is the list of headers
#
sub make_headers {
    my $hdrs = shift;
    my %h = ();
    my @hdrs = split(/\r?\n/,$hdrs);
    my $status = shift @hdrs || "HTTP/1.0 200 OK";
    for (@hdrs) {
        $h{$1} = $2 if /^([^:]+):\s*(.*)/;
    }
    return (new TACCutils::Headers($status,%h));
}

=item $hdrs_object = new TACCutils::Headers($status_line, $hdrs_as_string);

Given the status line and remaining headers separately, makes a
TACCutils::Headers object.

Note that the as_string method is overridden from HTTP::Headers.
Specifically, it outputs the status line as well as the headers proper,
it separates the headers with \r\n as demanded by the HTTP spec, and it
terminates the headers with a blank line.  The output of as_string is
appropriate to return as a distiller result.

=cut

sub TACCutils::Headers::new {
    my ($class,$status,%hdrs) = @_;
    my $h = new HTTP::Headers(%hdrs);
    $h->{"status"} = $status;
    return bless($h,$class);
}

=item $status = $hdrs_object->status([$new_status]);

Get (or,with an argument, set) the value of the header line.  Usually
it's something like "HTTP/1.0 200 OK".  You can use the status_ok
function (above) to check if the headers indicate a successful response.


=cut


sub TACCutils::Headers::status {
    my $self = shift;
    my $arg = shift;
    return undef if ref($self) ne 'TACCutils::Headers';
    if (defined $arg) {
        chop $arg while ($arg =~ /[\r\n]$/);
        $self->{"status"} = $arg;
    }
    return $self->{"status"};
}

sub TACCutils::Headers::DESTROY {
    my $self=shift;
  HTTP::Headers::DESTROY($self);
}

sub TACCutils::Headers::as_string {
    my $self = shift;
    $_ = HTTP::Headers::as_string($self);
    s/\n/\r\n/mg;
    my $str = join("\r\n", $self->status, $_);
    $str .= "\r\n" unless $str =~ /\r\n\r\n$/s;
}


#
#  Return the size in pixels of (the first image encoded in) a jpeg or gif
#    file.  
#

=back

=head2 Miscellaneous

=over 4

=item ($x,$y) = &jpeg_size($data); 

=item ($x,$y) = &gif_size($data);

Return the X and Y size in pixels for the (first image encoded in) the
given data, which should be the contents of a JFIF or GIF file (or at
least the first few hundred bytes of it).  It is ok for $data to be a
reference to the actual data (a scalar).

=cut

sub gif_size {
    my $data = shift;
    return (ref($data)
            ? (unpack("v2", substr($$data,6,4)))
            : (unpack("v2", substr($data,6,4))));
}

sub jpeg_size {
    my $data = shift;
    my($M_SOF0) = 0xC0;
    my($M_SOF15) = 0xCF;
    my($M_SOI) = 0xD8;
    my($M_EOI) = 0xD9;
    my($M_SOS) = 0xDA;
    my($M_COM) = 0xFE;
    my ($c1,$l,$d,$w,$h);
    $data = $$data if ref($data);

    my ($x,$y) = (-1,-1);
    ($x,$y) = unpack("C2", $data);
    substr($data,0,2) = '' if length($data) >= 2;
    return (0,0) unless ($x == 0xff && $y == $M_SOI); # 0xff, M_SOI
    while (length($data)) {
        ($c1,$d1) = unpack("C2", $data);
        $data = substr($data,2);
        return (0,0) unless $c1 == 0xff;
        if ($d1 >= $M_SOF0 && $d1 <= $M_SOF15) {
            ($l,$d,$h,$w) = unpack("nCnn", $data);
            return ($w,$h);
        } elsif ($c1 == $M_SOS || $c1 == $M_EOI) {
            last;
        } else {
            $l = unpack("n", $data);
            $data = substr($data, $l);
        }
    }
    return (0,0);
}
        

#########################################################
# but wait! load up the %entity mappings enwrapped in 
# a BEGIN that the last might be first, and only execute
# once, since we're in a -p "loop"; awk is kinda nice after all.
#########################################################

BEGIN {

    %TACCutils::entity = (

        'lt'     => '<',     #a less-than
        'gt'     => '>',     #a greater-than
        'amp'    => '&',     #a nampersand
        'quot'   => '"',     #a (verticle) double-quote

        'nbsp'   => chr 160, #no-break space
        'iexcl'  => chr 161, #inverted exclamation mark
        'cent'   => chr 162, #cent sign
        'pound'  => chr 163, #pound sterling sign CURRENCY NOT WEIGHT
        'curren' => chr 164, #general currency sign
        'yen'    => chr 165, #yen sign
        'brvbar' => chr 166, #broken (vertical) bar
        'sect'   => chr 167, #section sign
        'uml'    => chr 168, #umlaut (dieresis)
        'copy'   => chr 169, #copyright sign
        'ordf'   => chr 170, #ordinal indicator, feminine
        'laquo'  => chr 171, #angle quotation mark, left
        'not'    => chr 172, #not sign
        'shy'    => chr 173, #soft hyphen
        'reg'    => chr 174, #registered sign
        'macr'   => chr 175, #macron
        'deg'    => chr 176, #degree sign
        'plusmn' => chr 177, #plus-or-minus sign
        'sup2'   => chr 178, #superscript two
        'sup3'   => chr 179, #superscript three
        'acute'  => chr 180, #acute accent
        'micro'  => chr 181, #micro sign
        'para'   => chr 182, #pilcrow (paragraph sign)
        'middot' => chr 183, #middle dot
        'cedil'  => chr 184, #cedilla
        'sup1'   => chr 185, #superscript one
        'ordm'   => chr 186, #ordinal indicator, masculine
        'raquo'  => chr 187, #angle quotation mark, right
        'frac14' => chr 188, #fraction one-quarter
        'frac12' => chr 189, #fraction one-half
        'frac34' => chr 190, #fraction three-quarters
        'iquest' => chr 191, #inverted question mark
        'Agrave' => chr 192, #capital A, grave accent
        'Aacute' => chr 193, #capital A, acute accent
        'Acirc'  => chr 194, #capital A, circumflex accent
        'Atilde' => chr 195, #capital A, tilde
        'Auml'   => chr 196, #capital A, dieresis or umlaut mark
        'Aring'  => chr 197, #capital A, ring
        'AElig'  => chr 198, #capital AE diphthong (ligature)
        'Ccedil' => chr 199, #capital C, cedilla
        'Egrave' => chr 200, #capital E, grave accent
        'Eacute' => chr 201, #capital E, acute accent
        'Ecirc'  => chr 202, #capital E, circumflex accent
        'Euml'   => chr 203, #capital E, dieresis or umlaut mark
        'Igrave' => chr 204, #capital I, grave accent
        'Iacute' => chr 205, #capital I, acute accent
        'Icirc'  => chr 206, #capital I, circumflex accent
        'Iuml'   => chr 207, #capital I, dieresis or umlaut mark
        'ETH'    => chr 208, #capital Eth, Icelandic
        'Ntilde' => chr 209, #capital N, tilde
        'Ograve' => chr 210, #capital O, grave accent
        'Oacute' => chr 211, #capital O, acute accent
        'Ocirc'  => chr 212, #capital O, circumflex accent
        'Otilde' => chr 213, #capital O, tilde
        'Ouml'   => chr 214, #capital O, dieresis or umlaut mark
        'times'  => chr 215, #multiply sign
        'Oslash' => chr 216, #capital O, slash
        'Ugrave' => chr 217, #capital U, grave accent
        'Uacute' => chr 218, #capital U, acute accent
        'Ucirc'  => chr 219, #capital U, circumflex accent
        'Uuml'   => chr 220, #capital U, dieresis or umlaut mark
        'Yacute' => chr 221, #capital Y, acute accent
        'THORN'  => chr 222, #capital THORN, Icelandic
        'szlig'  => chr 223, #small sharp s, German (sz ligature)
        'agrave' => chr 224, #small a, grave accent
        'aacute' => chr 225, #small a, acute accent
        'acirc'  => chr 226, #small a, circumflex accent
        'atilde' => chr 227, #small a, tilde
        'auml'   => chr 228, #small a, dieresis or umlaut mark
        'aring'  => chr 229, #small a, ring
        'aelig'  => chr 230, #small ae diphthong (ligature)
        'ccedil' => chr 231, #small c, cedilla
        'egrave' => chr 232, #small e, grave accent
        'eacute' => chr 233, #small e, acute accent
        'ecirc'  => chr 234, #small e, circumflex accent
        'euml'   => chr 235, #small e, dieresis or umlaut mark
        'igrave' => chr 236, #small i, grave accent
        'iacute' => chr 237, #small i, acute accent
        'icirc'  => chr 238, #small i, circumflex accent
        'iuml'   => chr 239, #small i, dieresis or umlaut mark
        'eth'    => chr 240, #small eth, Icelandic
        'ntilde' => chr 241, #small n, tilde
        'ograve' => chr 242, #small o, grave accent
        'oacute' => chr 243, #small o, acute accent
        'ocirc'  => chr 244, #small o, circumflex accent
        'otilde' => chr 245, #small o, tilde
        'ouml'   => chr 246, #small o, dieresis or umlaut mark
        'divide' => chr 247, #divide sign
        'oslash' => chr 248, #small o, slash
        'ugrave' => chr 249, #small u, grave accent
        'uacute' => chr 250, #small u, acute accent
        'ucirc'  => chr 251, #small u, circumflex accent
        'uuml'   => chr 252, #small u, dieresis or umlaut mark
        'yacute' => chr 253, #small y, acute accent
        'thorn'  => chr 254, #small thorn, Icelandic
        'yuml'   => chr 255, #small y, dieresis or umlaut mark
    );

    ####################################################
    # now fill in all the numbers to match themselves
    ####################################################
    for $chr ( 0 .. 255 ) { 
        $entity{ '#' . $chr } = chr $chr;
    }
}



   1;
   
