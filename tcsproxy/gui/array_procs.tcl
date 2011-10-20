#
#  new_field, update_field, and script procedures for the
#  Array function
#  (display a small array of numbered indicators, each of which can
#  display one of N possible sequentially-numbered states, 0...N-1)

proc Array_new_field { frame unit field max_indicators width} {
    label $frame.name -text $field
    frame $frame.indicators
    frame $frame.legend

    set butSize 10

    if {$width == 0} {
        set width [expr ceil(sqrt($max_indicators))]        
    }
    set height [expr ceil($max_indicators/$width)]

    canvas $frame.c -width [expr $butSize*$width + 4] \
	    -height [expr $butSize*$height + 4]
    set x 0
    set col [Array_color $frame 0]
    for {set j 0} {$j < $height  &&  $x < $max_indicators} {incr j} {
        for {set i 0} {$i < $width  &&  $x < $max_indicators} \
		{incr i ; incr x} {
            set rectx [expr $i*$butSize]
            set recty [expr $j*$butSize]
            $frame.c create rectangle \
                    $rectx $recty \
                    [expr $rectx+$butSize-1] [expr $recty+$butSize-1] \
                    -fill $col \
                    -tags t$x
        }
    }
    
    label $frame.legend.b0 -bg [Array_color $frame 0] -text "thr_idle" \
	    -fg white -anchor c -pady 0
    grid $frame.legend.b0 -row 0 -column 0 -sticky nsew \
	    -ipadx 0 -ipady 0 -padx 1 -pady 1
    label $frame.legend.b1 -bg [Array_color $frame 1] -text "thr_accepted" \
	    -fg white -anchor c -pady 0
    grid $frame.legend.b1 -row 0 -column 1 -sticky nsew \
	    -ipadx 0 -ipady 0 -padx 1 -pady 1
    label $frame.legend.b2 -bg [Array_color $frame 2] -text "thr_cache" \
	    -fg white -anchor c -pady 0
    grid $frame.legend.b2 -row 0 -column 2 -sticky nsew \
	    -ipadx 0 -ipady 0 -padx 1 -pady 1
    label $frame.legend.b3 -bg [Array_color $frame 3] -text "thr_distiller" \
	    -fg white -anchor c -pady 0
    grid $frame.legend.b3 -row 1 -column 0 -sticky nsew \
	    -ipadx 0 -ipady 0 -padx 1 -pady 1
    label $frame.legend.b4 -bg [Array_color $frame 4] -text "thr_writeback" \
	    -fg white -anchor c -pady 0
    grid $frame.legend.b4 -row 1 -column 1 -sticky nsew \
	    -ipadx 0 -ipady 0 -padx 1 -pady 1
    label $frame.legend.b5 -bg [Array_color $frame 5] \
	    -text "thr_distillersend" -fg white -anchor c -pady 0
    grid $frame.legend.b5 -row 1 -column 2 -sticky nsew \
	    -ipadx 0 -ipady 0 -padx 1 -pady 1
    label $frame.legend.b6 -bg [Array_color $frame 6] -text \
	    "thr_distillerwait" -fg white -anchor c -pady 0
    grid $frame.legend.b6 -row 2 -column 0 -sticky nsew \
	    -ipadx 0 -ipady 0 -padx 1 -pady 1
    label $frame.legend.b7 -bg [Array_color $frame 7] -text "thr_state_max" \
	    -fg white -anchor c -pady 0
    grid $frame.legend.b7 -row 2 -column 1 -sticky nsew \
	    -ipadx 0 -ipady 0 -padx 1 -pady 1

    pack $frame.name -side top -anchor w -fill x
    pack $frame.c -side top -fill both -expand 1
    pack $frame.legend -side top -fill x
    pack $frame -side top -expand true -fill both
}

proc Array_update_field { frame unit field value sender } {
    set vals [split $value ","]
    set which_but 0
    foreach v $vals {
        if [regexp  {^([0-9.]+)=([0-9.]+)$} "$v" dummy b n] {
            set which_but [expr int($b)]
            set new_val [expr int($n)]
        } elseif [regexp -- {^[0-9.]+$} $v n] {
            set new_val [expr int($n)]
        } else {
            continue
        }
        $frame.c itemconfigure t$which_but -fill [Array_color $frame $n]
        incr which_but
    }
}
    
proc Array_color {frame indx} {
    set colors {green yellow orange blue red brown magenta gray}
    return [lindex $colors [expr {$indx % [llength $colors]}]]
}

# procedure to be specified as script for Array messages:
# Array message will have the format:
#  Value:  "v0,v1,v2" i.e. state values for each indicator: sets all
# indicators
#  Each vi can either be a numeric value or number=value.
#  E.g. the arg list "3,5,7=4,6,9,13=10,20=0" would set indicator 0 to
# the value 3, indicator 1 to the value 5, indicators 7-9 to the values
# 4,6,9 respectively, indicator 13 to the value 10, indicator 20 to the
# value 0, and leave all remaining indicators unchanged.
#  Script Command: "Array <max_indicators> <max_val> <ncols>"
#   ncols is the number of columns wide to make the table of
# indicators.  If omitted, the table is made as square as possible.

proc Array { args } {
    set max_indicators [lindex $args 0]
    set max_val [lindex $args 1]
    set ncols [lindex $args 2]

    if {$ncols == ""} {set ncols 0}

    if {$max_indicators > 0 && $max_val > 1} {
        eval " proc new_field {frame unit field} { \
            Array_new_field \$frame \$unit \$field $max_indicators $ncols \
        }"
        proc update_field { frame unit field value sender } {
            Array_update_field $frame $unit $field $value $sender
        }
    }
}
