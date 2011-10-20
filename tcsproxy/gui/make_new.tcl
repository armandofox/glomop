#
#  Support procedures for monitor: make a new unit or new field definition
#

# Create a new unit in its own toplevel window

proc make_newunit {unit hostName pid}  {
    global toplevel_counter fields units hosts pids

    # in the top level window, each unit has a frame called .frame-X
    # (where X is a counter), and gets a new toplevel called .top-X.
    # .frame-X
    # .frame-X.button (the control button to show/hide unit's main window)
    # .frame-X.label (name of unit)
    # .frame-X.kill  (button that sends this unit a death packet)
    # Note that
    # the procedure "unitnumber" returns the counter number X of a unit
    # given the unit's frame, toplevel, etc.

    set toplevel [toplevel .top-$toplevel_counter -class Monitor]
    wm title $toplevel $unit
    wm resizable $toplevel true true
    do_withdraw $toplevel   ;#default to invisible window.

    set unit_frame [frame .frame.frame-$toplevel_counter -class Top]
    set hostLabel  [label $unit_frame.host -text "$hostName ($pid)" -borderwidth 2 \
	    -relief sunken -anchor w]
    set label [label  $unit_frame.label  -text $unit -borderwidth 2 \
	    -relief sunken -anchor w]
    set show [button $unit_frame.button -text "Show" -width 8 -pady 0]
    $show configure -command "restore_unit $toplevel $show"
    set kill [button $unit_frame.kill -text "Kill" -width 6 -pady 0]
    $kill configure -command "kill_command \{$unit\}"
    bind $toplevel <Unmap> "$show configure -text Show \
            -command \"restore_unit $toplevel $show\""
    bind $toplevel <Map> "$show configure -text Hide \
            -command \"withdraw_unit $toplevel $show\""
    bind $toplevel <Destroy> "destroy_unit $toplevel %W $unit_frame"

    pack $hostLabel -side left -fill x -expand 1 -anchor w
    pack $label -side left -fill x -expand 1 -anchor w
    pack $kill $show -side right -anchor e
    pack $unit_frame -fill x -expand 1

    set units($unit) $toplevel
    set hosts($unit) $hostName
    set pids($unit)  $pid
    incr toplevel_counter
    
    # if a "custom" newunit proc was supplied, call it; otherwise call
    #  the default one.

    if {[info procs new_unit] != ""} {
        new_unit $toplevel $unit
    } else {
        default_new_unit $toplevel $unit
    }
}

proc unitnumber { thing } {
    if [regexp -nocase -- {[^-]+-+([0-9]+)$} $thing dummy num] {
        return $num
    } else {
        return 0
    }
}

#  Create a new field for an existing unit

proc make_newfield {unit field} {
    # this is a new field
    global frame_counter fields units

    set frame [frame $units($unit).frame$frame_counter]
    set fields($unit--$field) $frame
    incr frame_counter
    
    # if a "custom" new_field proc was specified, call it; otherwise
    # call the default one.

    if {[info procs new_field] != ""} {
        new_field $frame $unit $field
    } else {
        default_new_field $frame $unit $field
    } 
}


# this is the procedure that is called if no new_unit procedure is defined
# in the script accompanying the body of the message
proc default_new_unit { toplevel unit } {
    frame $toplevel.bar
    set dismiss [button $toplevel.bar.button -text "Destroy" -pady 0 \
	    -highlightthickness 0 -borderwidth 2 -command "destroy $toplevel"]
    set withdraw [button $toplevel.bar.button2 -text "Hide" -pady 0 \
            -highlightthickness 0 -borderwidth 2 -command "do_withdraw $toplevel"]
    pack $dismiss $withdraw -side right
    pack $toplevel.bar -side bottom -expand 1 -fill x
}

# this is the procedure that is called if no new_field procedure is defined
# in the script accompanying the body of the message
proc default_new_field { frame unit field } {
    label $frame.id     -width 24 -text $field
    label $frame.sender -width 16
    label $frame.value  -width 15 -fg red

    $frame configure -borderwidth 2 -relief sunken

    pack $frame.id $frame.sender $frame.value \
            -side left -fill none -expand 1
    pack $frame -side bottom -fill x -expand 1
}

# this is the procedure that is called if no update_field procedure is defined
# in the script accompanying the body of the message
proc default_update_field { frame unit field value sender } {
    set time [clock format [clock seconds] -format %H:%M:%S]
    $frame.sender configure -text $time
    $frame.value  configure -text $value
}


proc Death_new_field { frame unit field } {
    label $frame.text -relief sunken -borderwidth 2 -text "Unit died" \
	    -anchor w
    pack  $frame.text -fill x -expand 1 -padx 1
    pack  $frame -side bottom -fill x -expand 1
}


proc Death_update_field { frame unit field value sender } {
    if { $value == "0" } {
	$frame.text configure -text "Unit exited normally"
    } else {
	$frame.text configure -text "Unit died on signal $value"
    }
}


proc Death { } {
    proc new_field { frame unit field } {
	Death_new_field $frame $unit $field
    }

    proc update_field { frame unit field value sender } {
	Death_update_field $frame $unit $field $value $sender
    }
}


proc do_withdraw { toplevel } {
    if { [option get .frame noicons {}] == "yes" } {
	wm withdraw $toplevel
    } else {
	wm iconify $toplevel
    }
}

proc withdraw_unit { toplevel button } {
    do_withdraw $toplevel
    $button configure -text "Show" -command "restore_unit $toplevel $button"
}


proc restore_unit { toplevel button } {
    wm deiconify $toplevel
    $button configure -text "Hide" \
	    -command "withdraw_unit $toplevel $button"
}


proc destroy_unit { toplevel widget unit_frame } {
    if { $widget == $toplevel } { destroy $unit_frame }
}
