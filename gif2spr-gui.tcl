#!/usr/bin/env wish

package require Tk

set DISABLED_COLOR [ttk::style lookup TLabel -foreground disabled]

set game quake
set origin 0.5,0.5
set alignment vp-parallel
set gifPath ""
set palPath ""
set blendMode normal
set color #ffffff

proc colorButtonFlat {btn clr} {
    $btn configure -background $clr -highlightbackground $clr\
        -activebackground $clr
}

proc onSetBlendModeOrGame {} {
    if {$::blendMode == "index-alpha" && $::game == "hl"} {
        .l.colorlbl state !disabled
        colorButtonFlat .l.colorborder.btn $::color
        .l.colorborder.btn configure -state normal
    } else {
        .l.colorlbl state disabled
        colorButtonFlat .l.colorborder.btn $::DISABLED_COLOR
        .l.colorborder.btn configure -state disabled
    }
}

proc onSetBlendMode {args} {
    onSetBlendModeOrGame
}

proc onSetGame {args} {
    if {$::game == "hl"} {
        .l.blendlbl state !disabled
        .l.blendcombo state !disabled
    } else {
        .l.blendlbl state disabled
        .l.blendcombo state disabled
    }
    onSetBlendModeOrGame
}

proc onSetColor {args} {
    colorButtonFlat .l.colorborder.btn $::color
}

proc onSetGifPath {args} {
    if {[string trim $::gifPath] == ""} {
        .bot.write state disabled
    } else {
        .bot.write state !disabled
    }
}

proc writeSpr {} {
    set sprFile [tk_getSaveFile]
    if {[string trim $sprFile] == ""} {
        return
    }

    set cmd [list exec gif2spr -$::game -origin $::origin]
    lappend cmd -alignment $::alignment
    if {$::palPath != ""} {
        lappend cmd -palette "$::palPath"
    }
    lappend cmd "$::gifPath" "$sprFile"

    try {
        eval $cmd
    } trap CHILDSTATUS {err _} {
        tk_messageBox -type ok -title Error -message $err -icon error
    }
}

proc setColor args {
    set newColor [tk_chooseColor -initialcolor $::color]
    if {$newColor != ""} {
        set ::color $newColor
    }
}

trace add variable game write onSetGame
trace add variable blendMode write onSetBlendMode
trace add variable color write onSetColor
trace add variable gifPath write onSetGifPath

grid [ttk::frame .l] [ttk::frame .r]
grid [ttk::frame .bot] -columnspan 2 -sticky e

grid [ttk::label .l.gamelbl -text Game]\
    [ttk::frame .l.gameframe]

grid [ttk::radiobutton .l.gameframe.quake -variable game -text Quake\
    -value quake] [ttk::radiobutton .l.gameframe.hl -variable game\
    -text Half-Life -value hl]
grid configure .l.gameframe.hl -padx 18

grid [ttk::label .l.giflbl -text "Input GIF"]\
    [ttk::frame .l.gifframe]

grid [ttk::entry .l.gifframe.entry -textvariable gifPath -width 30]\
    [ttk::button .l.gifframe.btn -text Select -command {
        set ::gifPath [tk_getOpenFile]
    }]

grid [ttk::label .l.alignlbl -text Alignment]\
    [ttk::combobox .l.aligncombo -state readonly -textvariable alignment\
    -values {
        vp-parallel-upright
        upright
        vp-parallel
        oriented
        vp-parallel-oriented
    }]

grid [ttk::label .l.pallbl -text "Override Palette\n(Blank = Use Default)"\
    -justify right]\
    [ttk::frame .l.palframe]

grid [ttk::entry .l.palframe.entry -textvariable palPath -width 30]\
    [ttk::button .l.palframe.selbtn -text Select -command {
        set ::palPath [tk_getOpenFile]
    }]\
    [ttk::button .l.palframe.clrbtn -text Clear -command {
        set ::palPath ""
    }]

grid [ttk::label .l.blendlbl -text "Blend Mode"]\
    [ttk::combobox .l.blendcombo -state readonly -textvariable blendMode\
    -values {
        normal
        additive
        index-alpha
        alpha-test
    }]

grid [ttk::label .l.colorlbl -text "Color"]\
    [ttk::frame .l.colorborder -borderwidth 1 -relief sunken]
grid [button .l.colorborder.btn -width 8 -command setColor -state disabled\
    -borderwidth 0] -sticky nesw
grid columnconfigure .l.colorborder all -uniform c -weight 1 
grid rowconfigure .l.colorborder all -uniform r -weight 1

foreach widget [grid slaves .l -column 0] {
    grid configure $widget -sticky e
}

foreach widget [grid slaves .l -column 1] {
    grid configure $widget -sticky ew
}

grid configure .l.colorborder -sticky nsw

foreach widget [grid slave .l] {
    grid configure $widget -padx 4 -pady 4
}

grid [ttk::labelframe .r.origin -text Origin] -padx 4 -pady 4

grid [ttk::radiobutton .r.origin.nw -variable origin -text NW -value 0.0,1.0]\
    [ttk::radiobutton .r.origin.n -variable origin -text N -value 0.5,1.0]\
    [ttk::radiobutton .r.origin.ne -variable origin -text NE -value 1.0,1.0]
grid [ttk::radiobutton .r.origin.w -variable origin -text W -value 0.0,0.5]\
    [ttk::radiobutton .r.origin.c -variable origin -text Center -value 0.5,0.5]\
    [ttk::radiobutton .r.origin.e -variable origin -text E -value 1.0,0.5]
grid [ttk::radiobutton .r.origin.sw -variable origin -text SW -value 0.0,0.0]\
    [ttk::radiobutton .r.origin.s -variable origin -text S -value 0.5,0.0]\
    [ttk::radiobutton .r.origin.se -variable origin -text SE -value 1.0,0.0]

foreach radio [grid slave .r.origin] {
    grid configure $radio -padx 4 -pady 4 -sticky w
}

grid columnconfigure .r.origin all -uniform c -weight 1 -pad 4

grid [ttk::button .bot.write -text Write -command writeSpr]\
    [ttk::button .bot.quit -text Quit -command {destroy .}]\
    -padx 4 -pady 4 
.bot.write state disabled

onSetGame
. configure -padx 4 -pady 4
# set root background color to themed frame background color
. configure -background [ttk::style lookup TFrame -background]
wm resizable . 0 0
wm title . "gif2spr GUI Front-End"
