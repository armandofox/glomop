#
#  new_field, update_field, and script procedures for the
#  Log function
#

# new_field procedure for Log messages
proc Log_new_field { frame unit field width height maxLines} {
    label $frame.id -text $field
    set maxLinesVar $frame.text.maxLines
    global $maxLinesVar

#    set arg {}
#    if { $width != "" } { set arg "$arg -width $width" }
#    if { $height!= "" } { set arg "$arg -height $height" }
#    set $maxLinesVar $maxLines
#
#    if { $arg != "" } {
#	text $frame.text $arg
#    } else {
#        text $frame.text
#    }

    if { $width == "" } { set width  80 }
    if { $height== "" } { set height 10 }
    text $frame.text -width $width -height $height
    set $maxLinesVar $maxLines

    $frame.text configure \
            -font fixed \
            -xscrollcommand "$frame.sx set" \
            -yscrollcommand "$frame.sy set" -wrap none -state disabled

    bind $frame.text <Destroy> "catch \{ unset $maxLinesVar \}"
    scrollbar $frame.sx -orient horizontal -width 10 \
            -command "$frame.text xview"
    scrollbar $frame.sy -orient vertical -width 10 \
            -command "$frame.text yview"

    pack $frame.id -side top -anchor w
    pack $frame.sx -side bottom -fill x
    pack $frame.sy -side right  -fill y
    pack $frame.text -side left -fill both -expand true
    pack $frame -expand true -fill both
}

# update_field procedure for Log messages
proc Log_update_field { frame unit field value sender } {
    set maxLinesVar $frame.text.maxLines
    global $maxLinesVar
    set maxLines [set $maxLinesVar]
    $frame.text configure -state normal
    $frame.text insert end \
            "[clock format [clock seconds] -format %H:%M:%S] $value"
    $frame.text see end
    set totalLines [lindex [split [$frame.text index end] .] 0]
    if { $totalLines > $maxLines } {
	$frame.text delete 1.0 [expr $totalLines - $maxLines].0
    }

    $frame.text configure -state disabled
}


# the procedure to be specified as the script for log messages
# i.e. a log message will have the format:
# <unit>\001<field>\001<value\001Log
#    or                          ^^^
# <unit>\001<field>\001<value\001Log <width> <height> <maxLines>
#                                ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
proc Log { args } {
    set width    "[lindex $args 0]"
    set height   "[lindex $args 1]"
    set maxLines "[lindex $args 2]"
    if { $maxLines == "" } { set maxLines 100 }

    set new_field_body "Log_new_field \$frame \$unit \$field \{$width\} \
	    \{$height\} \{$maxLines\}"
    proc new_field { frame unit field } $new_field_body

    proc update_field { frame unit field value sender } {
	Log_update_field $frame $unit $field $value $sender
    }
}

