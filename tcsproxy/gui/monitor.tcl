#!/bin/sh
# I'm using this hack so that I can rely on the system using the PATH
# environment variable to locate wish
#
# the exec restarts using wish which in turn ignores
# the command because of this backslash: \
exec wish "$0" -name monitor "$@"



# Debugging info consists of a unit name, field name, value and
# script.  The components are separated by \001 (or the value of the
# global $Sep, set in the init procedure).  The value can be
# any arbitrary string (it may not contain the character \001). Case and
# whitespace are significant!! Don't put any whitespace around
# \001's unless you want the whitespace to be seen as part of the strings!!
#
#  unit name:  string decribing who sent the info, e.g. PTM, FE, CACHE,
#   etc.  
#  field name: string describing what's being reported, e.g. "Threads
#       idle"
#  value: the actual data.  Can be any arbitrary string of characters
#
#  script: arbitrary Tcl script that is executed when the message is received
#       Typically, this script should be the name of one of the predefined
#       scripts. Currently only 3 types of scripts are defined:
#          the default script: used when no script is specified.
#               Displays the field name/value pair using label widgets,
#               along with the IP address of the last updater.
#          Log: this script is used to output log messages to a scrollable 
#               window
#          TimeChart: plot values continuously a la "xload"
#       In general the script should define all or some of the following 
#       procedures:
#          proc new_unit { toplevel unit } { ... }
#             this proc is invoked when the unit is encountered for the first
#             time. "toplevel" is the pathname of a new toplevel window
#             that the monitor automatically creates for this unit
#          proc new_field { frame unit field } { ... }
#             this proc is invoked when the field is encountered for the first
#             time. "frame" is the pathname of a new frame that the monitor
#             automatically creates for this field. The monitor does not
#             pack this frame. It is up to this procedure to do the packing
#          proc update_field { frame unit field value sender } { ... }
#             this proc is invoked to update the value of the field
#             sender refers to the network address of the entity that
#             sent this message
#
#  To define your own default scripts, use the definition of the Log script
#  towards the end of this file as a guide.
#


proc init {argv0 options_file} {
    global toplevel_counter frame_counter
    global Udp Sep

    set Sep "\001"              ;#separator for message components

    set toplevel_counter 0
    set frame_counter 0
    
    # display options
    
    option add *foreground black
    option add *background #d9d9d9
    if [catch {option readfile $options_file} msg] {
        puts stderr "Warning: problems reading $options_file: $msg"
    }

    # basic window
    frame  .frame -class Top -borderwidth 2 -relief ridge
    frame  .bar -class monitor
    label  .bar.label -text "   $argv0 v[version]   "
    button .bar.quit -text "Quit Monitor" -command "destroy ."
    button .bar.killall -text "Kill All" -command "killall"
    pack .bar.label -side left
    pack .bar.quit -side right -anchor w
    pack .bar.killall -side right
    pack .frame -side top -expand 1 -fill both
    pack .bar -side bottom -expand 1 -fill x
    catch "wm resizable . true true"

    # source other files

    source "make_new.tcl"

    # source procedures for built-in monitoring types

    foreach file [glob -nocomplain -- "./*_procs.tcl"] {
        catch {source $file}
    }

    # load UDP multicast listening support

    load "./tudp.so"

    set mlisten [option get . listen Monitor]
    if {$mlisten == ""} {
        puts stderr "No option Monitor.listen found in $options_file!"
        exit 1
    }
    #if [regexp "/.+/" $mlisten] {
        # multicast addr/port/ttl
        #set Udp [udp_listen -multicast $mlisten -callback callback]
    #} else {
        # unicast port number
        #set Udp [udp_listen -unicast $mlisten -callback callback]
    #}
    set Udp [udp_listen -address $mlisten -callback callback]
    .bar.label configure -text "   $argv0 v[version]   $mlisten"
}

#
#  Kill all entities
#

proc killall {} {
    foreach unit_frame [winfo children .frame] {
        $unit_frame.kill invoke
    }
}

#
#  This is the main entry point that gets called whenever a packet
#  arrives
#

proc callback {fromaddr fromport data} {
    global fields units
    global Sep
    
    set data [split $data $Sep]
    if { [llength $data] < 4 } { return }
    set hostName [lindex $data 0]
    set pid      [lindex $data 1]
    set unit     [lindex $data 2]
    set field    [lindex $data 3]
    set value    [lindex $data 4]
    set procs    [lindex $data 5]

    if { [string index $procs 0] != "!" } {
        catch {eval $procs}
        # $procs might contain definitions for new_unit, new_field, and
        # update_field procs
    }
    #
    #  If this is the echo of a "kill yourself" packet sent to a unit,
    #  ignore it.
    #
    if {[string compare $field "%KILL%"] == 0 \
            || [string compare $value "%KILL%"] == 0 } {
        return
    }
    
    #
    #  If this is a "death packet" from a unit....destroy unit's window.
    #
    if { [string compare $field "%DEATH%"] == 0 \
            || [string compare $value "%DEATH%"] == 0} {

	if { ! [info exists units($unit)] } { return }

        if { [option get .frame destroyondeath {}] == "yes" } {
	    catch {destroy $units($unit)}
	    return
	} else {
	    wm title $units($unit) "$unit (Dead)"

	    set unit_frame .frame.frame-[unitnumber $units($unit)]
	    foreach child [winfo children $unit_frame] {
		$child configure -background gray
	    }
	    #$unit_frame.kill configure -state disabled
	    $unit_frame.kill configure \
		-command "destroy_unit $units($unit) $units($unit) $unit_frame" 

	    Death
	}
    }

    #
    #  Otherwise process normal packet.
    #
    if { [info exists units($unit)] && (![winfo exists $units($unit)]) } { 
	unset units($unit)
    }
    if { [info exists fields($unit--$field)] && \
	    ![winfo exists $fields($unit--$field)] } { 
	unset fields($unit--$field)
    }

    if { ! [info exists units($unit)] } {
        make_newunit $unit $hostName $pid
    }

    if {! [info exists fields($unit--$field)]} {
        make_newfield $unit $field
    }

    # if a custom update-field proc was supplied, call it; otherwise
    #  call the default one.

    set frame $fields($unit--$field)
    if {[info procs update_field] != ""} {
	catch {update_field $frame $unit $field $value "$fromaddr/$fromport"}
    } else {
	catch {default_update_field $frame $unit $field $value \
                "$fromaddr/$fromport"} 
    } 

    # delete these procedures.
    # BUG::This is the wrong way to do it.  Should keep them around per
    # unit.  This way has too much overhead.
    
    foreach x {new_unit new_field update_field} {
        if {[info procs $x] != ""} { rename $x {} }
    }
}

# send a "kill yourself" packet to a given unit, to tell it to commit
# suicide.  When it dies, it should try to send a "last gasp" packet,
# which is a packet whose "field" value is "%DEATH%".

proc death_packet {unit} {
    global Udp Sep
    udp_send $Udp "${unit}${Sep}%KILL%"
}


proc kill_command {unit} {
    global hosts pids units
    global myhostname

    if {! [info exists myhostname]} {
        catch {set myhostname [exec hostname]}
    }

    # puts "*** killing $pids($unit) with $cmd ***"
    if {$myhostname != $hosts($unit)}  {
        set rsh [option get .frame rsh {}]
        if {$rsh == ""} { set rsh "rsh" }
	set cmd "kill -TERM $pids($unit); sleep 5; kill -KILL $pids($unit)"
	catch {exec $rsh -n $hosts($unit) $cmd > /dev/null 2> /dev/null &}

        #catch "exec $rsh $hosts($unit) kill -TERM $pids($unit)"
        #after 5000 "catch {exec $rsh $hosts($unit) kill -KILL $pids($unit)}"
    } else {
        catch "exec kill -TERM $pids($unit) 2>/dev/null  &"
        after 5000 "catch {exec kill -KILL $pids($unit) 2> /dev/null &}"
    }

    wm title $units($unit) "$unit (Killing)"
    
    set unit_frame .frame.frame-[unitnumber $units($unit)]
    foreach child [winfo children $unit_frame] {
	$child configure -background green
    }

    $unit_frame.kill configure \
            -command "destroy_unit $units($unit) $units($unit) $unit_frame" 
}


proc version {} {
    # DO NOT MODIFY THE FOLLOWING LINE -- it is generated by CVS
    set rev {$Revision: 1.33 $}
    if [regexp -- {([0-9.]+)} $rev revnumber] {
        return $revnumber
    } else {
        return "??"
    }
}

cd [file dirname $argv0]
set argv0 [file tail $argv0]
tk appname monitor
init $argv0 [lindex $argv 0]
