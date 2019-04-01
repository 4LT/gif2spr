#!/usr/bin/env wish

package require Tk 8.6
package require platform

# Boilerplate Section

proc followLinks {path} {
    while 1 {
        try {
            set dir [file dirname $path]
            set path [file join $dir [file readlink $path]]
        } on error {} {
            break
        }
    } 
    return $path
}

set OS [regsub -- {-.*$} [platform::identify] ""]
set DISABLED_COLOR [ttk::style lookup TLabel -foreground disabled]
set SCRIPT_PATH [file normalize [followLinks [info script]]]

if {$OS == {win32}} {
    set WIN 1
    set MAC 0
    set NIX 0
} elseif {$OS == {cygwin}} {
    set WIN 1
    set MAC 0
    set NIX 1
} elseif {$OS == {linux}} {
    set WIN 0
    set MAC 0
    set NIX 1
} elseif {[string first macosx $OS] == 0} {
    set WIN 0
    set MAC 1
    set NIX 1
} else {
    set WIN 0
    set MAC 0
    set NIX 1
}

if {$MAC} {
    set APP 1
} else {
    set APP 0
}

if {$NIX && !$APP} { ;# Linux, Cygwin, cmd-line Mac
    if {[string first /usr/local [file normalize [info script]]] == 0} {
        set DATA_PATH /usr/local/share/gif2spr
    } else {
        set DATA_PATH /usr/share/gif2spr
    }
    set USER_DATA_PATH [file join $env(HOME) .local share gif2spr]
    set GIF2SPR gif2spr
} elseif {$MAC} { ;# Mac application
    # ???
    throw UNIMPLEMENTED "Ouch! Mac stuff unimplemented"
    set USER_DATA_PATH [file join $env(HOME) Library "Application Support"\
        gif2spr]
} else { ;# Assume Windows
    set DATA_PATH [file dirname $SCRIPT_PATH]
    set USER_DATA_PATH [file join $env(USERPROFILE) AppData Local gif2spr]
    set GIF2SPR [file join $DATA_PATH gif2spr]
}

try {
    file mkdir $USER_DATA_PATH
} on error {} {
    puts stderr "Could not get/set user data path"
}

try {
    set initDictChan [open [file join $USER_DATA_PATH init.dict]]
    set initDict [read $initDictChan]
} on error {} {
    set initDict {
        dirs {}
        game quake
    }
} finally {
    if {[info exists initDictChan]} {
        close $initDictChan
    }
}

# UI Section

if {[dict exists $initDict game]} {
    set game [dict get $initDict game]
} else {
    set game quake
}
if {$argc > 0} {
    set gifPath [lindex $argv 0]
} else {
    set gifPath ""
}
set origin 0.5,0.5
set alignment vp-parallel
set palPath ""
set blendMode normal
set color #ffffff

proc onClose {} {
    try {
        set initDictChan [open [file join $::USER_DATA_PATH init.dict] w]
        puts $initDictChan $::initDict
    } on error {err} {
        puts stderr "Could not save init directories"
        puts $err
    } finally {
        if {[info exists initDirDictChan]} {
            close $initDictChan
        }

        destroy .
    }
}

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
    dict set ::initDict game $::game
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
    set saveFileScript tk_getSaveFile
    lappend saveFileScript -filetypes {
        {"Sprite" .spr}
        {"All Files" *}
    }
    if {[dict exists $::initDict dirs spr]} {
        lappend saveFileScript -initialdir [dict get $::initDict dirs spr]
    }
    set sprFile [eval $saveFileScript]

    if {[string trim $sprFile] == ""} {
        return
    } else {
        dict set ::initDict dirs spr [file dirname $sprFile]
    }

    set cmd [list exec $::GIF2SPR -$::game -origin $::origin]
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

proc selectFile {type pathVar} {
    upvar $pathVar myPathVar
    set openFileScript tk_getOpenFile
    set path ""

    if {$type == {gif}} {
        set gifTypes {
            {"GIF Files" .gif}
            {"All Files" *}
        }
        if {[file exists $::gifPath]} {
            lappend openFileScript -initialdir [file dirname $::gifPath]
        } elseif {[dict exists $::initDict dirs gif]} {
            lappend openFileScript -initialdir [dict get $::initDict dirs gif]
        }
        lappend openFileScript -filetypes $gifTypes
    } elseif {$type == {pal}} {
        if {[dict exists $::initDict dirs pal]} {
            lappend openFileScript -initialdir [dict get $::initDict dirs pal]
        }
    } else {
        throw {} "Unsupported file type \"$type\""
    }

    set path [eval $openFileScript]
    if {$path != {}} {
        dict set ::initDict dirs $type [file dirname $path]
        set $pathVar $path
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
        selectFile gif ::gifPath
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
        selectFile pal ::palPath
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
    [ttk::button .bot.quit -text Quit -command onClose]\
    -padx 4 -pady 4 
.bot.write state disabled

wm protocol . WM_DELETE_WINDOW {
    onClose
}

onSetGame
. configure -padx 4 -pady 4
# set root background color to themed frame background color
. configure -background [ttk::style lookup TFrame -background]
wm resizable . 0 0
wm title . "gif2spr GUI Front-End"
