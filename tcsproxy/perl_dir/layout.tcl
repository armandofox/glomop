#!/opt/bin/wish -f

# break_string should be a unique expression (ie that should "never occur" in
# the body text of an html page) that this script will use to denote where it
# thinks line breaks are indicated in the output.  

set break_string "<//>"

# check for fonts first....

if [catch {label .ll -font pilot-small} err] {
    error "Error: $err (did you remember to 'xset +fp ../pilotFonts'?)"
}

proc newwin {w h} {
    wm resizable . 1 1
    global status currFile
    frame .view
    scrollbar .view.s -orient vertical \
            -width 10 -command ".view.f yview"
    pack .view.s -side right -expand 1 -fill y -padx 0

    canvas .view.f \
            -width $w \
            -scrollregion "0 0 $w $h" \
            -relief flat \
            -yscrollcommand ".view.s set"
    global c
    set c ".view.f"
    clearCanv
    pack .view.f -side left -expand 1 -fill y -padx 0

    # control buttons
    frame .ctrl
    label .ctrl.status -font pilot-small \
            -height 2 -wraplength 160 \
            -textvariable status -anchor w \
            -text "Red=italics  Blue=link"
    pack .ctrl.status -side top -fill x -expand 0
    button .ctrl.open -text "Open..." -command { 
        if [openFile [set  f [tk_getOpenFile]]] {
            set currFile $f
        }
    }
    button .ctrl.reload -text "Reload" -command {openFile $currFile}
    button .ctrl.quit -text "Quit" -command exit
    pack .ctrl.open .ctrl.reload .ctrl.quit -side left -fill x -expand 1

    $c bind link <Enter> \
            "$c configure -cursor hand2"
    $c bind link <Leave> \
            "set status {} ; $c configure -cursor {}"

    pack .view -side top -fill y -expand 1
    pack .ctrl -side bottom -fill none -expand 1
    update
    return $c
}

proc openFile {file} {
    global currFile c
    if {[set f [open $file "r"]] != ""} {
        clearCanv
        readfile $f $c
        return 1
    } else {
        tk_dialog .dialog "File Open Error" "Unable to open '$file'"
        return 0
    }
}

proc clearCanv {} {
    global c
    global cnt maxY
    set cnt 0
    $c delete all
    set maxY 0
}

# this routine expects the format:
# x y xextent yextent tag

proc add {c x y xe ye attrs link tag} {
    global tags
    global cnt
    global status
    global maxY
    global break_string
    
    if {! [info exists cnt]} {
        set cnt 0
    }
    #puts stderr "Got $x $y $xe $ye $tag"
    if {[expr $y+$ye] > $maxY} {
        set maxY [expr $y+$ye]
    }
    regsub -all "{|}" $tag "\\&" tag
    if [regexp {\[image ([^ ]+)\]} $tag dummy imgfile] {
        # image item
        image create photo img$cnt -file $imgfile
        set img [$c create image $x $y -image img$cnt -anchor nw -tags link]
        if [expr $attrs & 0x40] { 
            # this is a link
            set item [$c create rectangle $x $y [expr $x+$xe] [expr $y+$ye] \
                    -outline blue -tags link]
            $c bind $img <Enter> "set status {$link}"
        }
        incr cnt
    } else {
        # text item:  draw rect & pick font
        regsub -all $break_string $tag "\n" tag
        set fn "pilot-small"
        if [expr $attrs & 1] {
            # bold
            set fn "pilot-bold"
        }
        if [expr $attrs & 8] {
            # could be FONT_BIG1 or FONT_BIG2, but for now we map both
            # of these to the single font "pilot-large"
            set fn "pilot-large"
        }
        set txt [$c create text $x $y -anchor nw -font $fn -text $tag]
        set item [$c create rectangle $x $y [expr $x+$xe-1] [expr $y+$ye-1] \
                -tags txt -stipple gray50 -outline {}]
        if [expr $attrs & 0x4] {
            # underline
            #$c create line $x [expr $y+$ye] [expr $x+$xe-1] [expr $y+$ye] \
                    # -stipple gray50
            $c itemconfigure $txt -fill red
        }
        if [expr $attrs & 0x40] {
            # link
            $c bind $item <Enter> "set status {$link}"
            $c itemconfigure $item -tags link
            $c itemconfigure $txt -fill blue
            #$c create line $x [expr $y+$ye] [expr $x+$xe-1] [expr $y+$ye] \
                    #-fill blue
        } else {
            $c bind $item <Button-1> "puts {$tag}"
        }
    }
}

proc readfile {fh canv} {
    while {[gets $fh s] != -1} {
        if [regexp \
                {([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +([^ ]+ +)?{(.*)}} $s dummy \
                x y xe ye attrs link tag] {
            add $canv $x $y $xe $ye $attrs $link $tag
        } else {
            tkerror "File format error: bad line '$s'"
            exit 1
        }
    }
    close $fh
    global maxY c
    $c configure -scrollregion "0 0 160 [expr 5+$maxY]"
}

global currFile
if {[set currFile [lindex $argv 0]] != ""} {
    set fh [open $currFile "r"]
} else {
    set fh "stdin"
}

set canv [newwin 160 3000]
readfile $fh $canv
tkwait window .
