/*
 * Arguments that all distillers (as well as the frontend) can possibly 
 * understand.
 *
 * FRONT_NO_DISTILL - integer argument
 *      If this argument is <> 0 then the distiller should not distill this
 *      request.
 *
 * FRONT_RESIZE - integer argument
 *      If this argument <> 0 then the distiller should resize any
 *      item that might be shruken, such as an image.  This argument
 *      won't be set by a user, but rather by the HTML distiller when
 *      it detects that an IMG tag has no width/height attributes.
 *      The distiller can either resize data back to the original, or
 *      not play with sizes at all and just crank on the quality knob.
 *
 * FRONT_MTU - the payload MTU for the user's network (e.g. 1500 or so
 *      for PPP).  In general, if content is less than this big, it
 *      doesn't pay to distill.
 *
 * FRONT_RETRY_DISTILL - the number of times to retry a distillation if the
 *      return code is distConnectionBroken (indicating distiller coredumped or
 *      died). This should be bounded, in order to avoid systematic killing of
 *      a distiller by the same request repeatedly.
 *
 * FRONT_QUALITY - this integer argument represents the "abstract"
 *      quality of an image, from (1,5) inclusive.  (1 is low/fast,
 *      5 is high/slow).
 *
 * FRONT_SHOW_ICON - controls user interface for quality-setting and refinement:
 *      0 - no visible user interface on pages
 *      1 - show Java dashboard (which uses Javascript "refine all")
 *      2 - show TranSend icon that links to Prefs page
 *
 * FRONT_EXPERT - if nonzero, the user is using the expert interface
 *
 * FRONT_ISMAP  - is an imagemap
 *
 * FRONT_BYPASS_TRANS - bypass transparencies
 *
 * FRONT_CLIENT_IP - INT32 representation of client IP address
 *
 * FRONT_USERID - short string, user ID of this user.  If nonempty, the
 *      distiller is chosen by prepending this name plus colon to the MIME
 *      type.  Username 'transend' are the system distillers, which are used if
 *      this arg is not given or empty string.
 *
 * FRONT_NOCACHE - integer - if present and value is 1, then send a no-cache
 *      to the server for this page and all inlines fetched from this page.
 *      If absent or 0, don't do anything special.
 *
 * FRONT_CLI_VERSION - integer - a code respresenting the client version, so
 *      we can do some things only for some client versions
 *
 * FRONT_DEVELOPER - integer - if nonzero, the user is a developer and we
 *      should explicitly report distiller routing errors, distiller not found,
 *      etc. rather than trying to mask it.
 *
 * FRONT_SRCFLAG - integer - if 1, view the "raw" version of this document
 *                           if 2, view the raw version as a Doc file
 *                           if 0, view the processed version
 */
#define FRONT_SIG 0

#define FRONT_NO_DISTILL      FRONT_SIG+1 /* int */
#define FRONT_RESIZE          FRONT_SIG+2 /* int */
#define FRONT_MTU             FRONT_SIG+3 /* int */
#define FRONT_RETRY_DISTILL   FRONT_SIG+4 /* int */
#define FRONT_QUALITY         FRONT_SIG+5 /* int */
#define FRONT_SHOW_ICON       FRONT_SIG+6 /* int */
#define FRONT_EXPERT          FRONT_SIG+7 /* int */
#define FRONT_ISMAP           FRONT_SIG+8 /* int */
#define FRONT_BYPASS_TRANS    FRONT_SIG+9 /* int */
#define FRONT_CLIENT_IP       FRONT_SIG+10 /* int */
#define FRONT_USERID          FRONT_SIG+11 /* string */
#define FRONT_ANONYMIZE       FRONT_SIG+12 /* int */
#define FRONT_NOCACHE         FRONT_SIG+13 /* int */
#define FRONT_CLI_VERSION     FRONT_SIG+14 /* int */
#define FRONT_DEVELOPER       FRONT_SIG+15 /* int */
#define FRONT_SRCFLAG         FRONT_SIG+16 /* int */
