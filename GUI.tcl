#!/bin/sh
# The next line restarts using wish \
exec wish $0 ${1+"$@"}

# GUI.tcl Copyright (C) 2003 Jochen Hoenicke.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
#
# $Id: GUI.tcl 222 2006-02-20 17:35:42Z hoenicke $

set firmware ""
set brickaddr 0
set brickosname bibo
set brickbothome "/usr/local"
if { [llength [array get env BRICKOS_DIR]] != 0 } {
    set brickbothome $env(BRICKOS_DIR)
}

set BRICKOS_LIBDIR "$brickbothome/lib/$brickosname"
if { [llength [array get env BRICKOS_LIBDIR]] != 0 } {
    set BRICKOS_LIBDIR $env(BRICKOS_LIBDIR)
}

set CROSSTOOLPREFIX "$brickbothome/bin/h8300-hitachi-coff-"
if { [llength [array get env CROSSTOOLPREFIX]] != 0 } {
    set CROSSTOOLPREFIX $env(CROSSTOOLPREFIX)
}
set emufd -1
set progs [list "" "" "" "" "" "" "" ""]



proc send_cmd { cmd } {
    global emufd; 
    if { $emufd != -1 } {
	#puts "send: $cmd"
	puts $emufd $cmd
	flush $emufd
    }
}

proc update_lcd { idx val } {
    global lcd_item;
    if {$idx == 99 } {
	if { $val } {
	    .row.display configure -bg grey90
	} else {
	    .row.display configure -bg grey30
	}
	return;
    }
    for {set i 0} { $i < 8} {incr i 1} {
	if {[llength [array get lcd_item [expr 8*$idx+$i]]]} {
	    set item $lcd_item([expr 8*$idx+$i]);
	    set color {}
	    if { $val & (1 << $i) } {
		set color black
	    }
	    .row.display itemconfigure $item -fill $color;
	}
    }
    update;
}    

proc create_display { } {
    global lcd_item;
    set width 250
    set height 90
    set line_width 4
    array set dig { 0 194 1 137 2 109 3 81 4 53 };
    canvas .row.display -width $width -height $height -bg grey30 -borderwidth 0;

    # The FIGURE
    set lcd_item(0) [ .row.display create polygon \
			  167 43 167 49 164 52 164 57 162 57 162 50 167 43 \
			  179 44 184 50 187 50 187 53 182 53 179 49 179 44 \
			  ];
    set lcd_item(1) [ .row.display create polygon \
			  171 28 175 28 175 30 177 30 178 34 179 35 178 36 \
			  178 39 176 42 178 44 178 60 175 64 171 64 168 60 \
			  168 44 170 42 168 39 168 31 169 30 171 30 171 28 ];
    set lcd_item(2) [ .row.display create polygon \
			  173 65 171 65 171 75 179 75 179 74 175 72 175 65 \
			  173 65 ];
    set lcd_item(3) [ .row.display create polygon \
			  168 62 161 71 165 75 167 75 165 71 170 64 168 62 \
			  176 65 182 75 188 72 187 71 183 70 178 63 176 65 ];

    # DIGIT 0
    set lcd_item(12) [ .row.display create line \
			   $dig(0) 29 [expr $dig(0)+15] 29 \
			   -width $line_width ];
    set lcd_item(37) [ .row.display create line \
			   $dig(0) 31 $dig(0) 44 \
			   -width $line_width ];
    set lcd_item(21) [ .row.display create line \
			   [expr $dig(0) + 17] 29 [expr $dig(0) + 17] 45 \
			   -width $line_width ];
    set lcd_item(13) [ .row.display create line \
			   [expr $dig(0) + 2] 46 [expr $dig(0)+15] 46 \
			   -width $line_width ];
    set lcd_item(39) [ .row.display create line \
			   $dig(0) 47 $dig(0) 61 \
			   -width $line_width ];
    set lcd_item(23) [ .row.display create line \
			   [expr $dig(0) + 17] 47 [expr $dig(0) + 17] 63 \
			   -width $line_width ];
    set lcd_item(15) [ .row.display create line \
			   $dig(0) 63 [expr $dig(0)+15] 63 \
			   -width $line_width ];
    
    # DIGIT 1
    set lcd_item(4)  [ .row.display create line \
			   $dig(1) 29 [expr $dig(1)+15] 29 \
			   -width $line_width ];
    set lcd_item(45) [ .row.display create line \
			   $dig(1) 31 $dig(1) 45 \
			   -width $line_width ];
    set lcd_item(33) [ .row.display create line \
			   [expr $dig(1) + 17] 29 [expr $dig(1) + 17] 45 \
			   -width $line_width ];
    set lcd_item(5)  [ .row.display create line \
			   [expr $dig(1) + 2] 46 [expr $dig(1)+15] 46 \
			   -width $line_width ];
    set lcd_item(47) [ .row.display create line \
			   $dig(1) 47 $dig(1) 61 \
			   -width $line_width ];
    set lcd_item(35) [ .row.display create line \
			   [expr $dig(1) + 17] 47 [expr $dig(1) + 17] 63 \
			   -width $line_width ];
    set lcd_item(7)  [ .row.display create line \
			   $dig(1) 63 [expr $dig(1)+15] 63 \
			   -width $line_width ];

    # DIGIT 2
    set lcd_item(8)  [ .row.display create line \
			   $dig(2) 29 [expr $dig(2)+15] 29 \
			   -width $line_width ];
    set lcd_item(53) [ .row.display create line \
			   $dig(2) 31 $dig(2) 45 \
			   -width $line_width ];
    set lcd_item(41) [ .row.display create line \
			   [expr $dig(2) + 17] 29 [expr $dig(2) + 17] 45 \
			   -width $line_width ];
    set lcd_item(9)  [ .row.display create line \
			  [expr $dig(2) + 2] 46 [expr $dig(2)+15] 46 \
			   -width $line_width ];
    set lcd_item(55) [ .row.display create line \
			   $dig(2) 47 $dig(2) 61 \
			 -width $line_width ];
    set lcd_item(43) [ .row.display create line \
			   [expr $dig(2) + 17] 47 [expr $dig(2) + 17] 63 \
			   -width $line_width ];
    set lcd_item(11) [ .row.display create line \
			   $dig(2) 63 [expr $dig(2)+15] 63 \
			   -width $line_width ];
    set lcd_item(46) [ .row.display create line \
			   [expr $dig(2)+21] 63 [expr $dig(2)+25] 63 \
			   -width $line_width ];


    # DIGIT 3
    set lcd_item(24) [ .row.display create line \
			   $dig(3) 29 [expr $dig(3)+15] 29 \
			   -width $line_width ];
    set lcd_item(69) [ .row.display create line \
			   $dig(3) 31 $dig(3) 45 \
			   -width $line_width ];
    set lcd_item(65) [ .row.display create line \
			   [expr $dig(3) + 17] 29 [expr $dig(3) + 17] 45 \
			   -width $line_width ];
    set lcd_item(25) [ .row.display create line \
			   [expr $dig(3) + 2] 46 [expr $dig(3)+15] 46 \
			   -width $line_width ];
    set lcd_item(71) [ .row.display create line \
			 $dig(3) 47 $dig(3) 61 \
			 -width $line_width ];
    set lcd_item(67) [ .row.display create line \
			 [expr $dig(3) + 17] 47 [expr $dig(3) + 17] 63 \
			 -width $line_width ];
    set lcd_item(27) [ .row.display create line \
			   $dig(3) 63 [expr $dig(3)+15] 63 \
			   -width $line_width ];
    set lcd_item(54) [ .row.display create line \
			   [expr $dig(3)+21] 63 [expr $dig(3)+25] 63 \
			   -width $line_width ];
    

    # DIGIT 4
    set lcd_item(28) [ .row.display create line \
			   $dig(4) 29 [expr $dig(4)+15] 29 \
			   -width $line_width ];
    set lcd_item(61) [ .row.display create line \
			   $dig(4) 31 $dig(4) 45 \
			   -width $line_width ];
    set lcd_item(57) [ .row.display create line \
			   [expr $dig(4) + 17] 29 [expr $dig(4) + 17] 45 \
			   -width $line_width ];
    set lcd_item(29) [ .row.display create line \
			   [expr $dig(4) + 2] 46 [expr $dig(4)+15] 46 \
			   -width $line_width ];
    set lcd_item(63) [ .row.display create line \
			   $dig(4) 47 $dig(4) 61 \
			   -width $line_width ];
    set lcd_item(59) [ .row.display create line \
			   [expr $dig(4) + 17] 47 [expr $dig(4) + 17] 63 \
			   -width $line_width ];
    set lcd_item(31) [ .row.display create line \
			   $dig(4) 63 [expr $dig(4)+15] 63 \
			   -width $line_width ];
    set lcd_item(70) [ .row.display create line \
			   [expr $dig(4)+21] 63 [expr $dig(4)+25] 63 \
			   -width $line_width ];


    # DIGIT 5 (minus)
    set lcd_item(62) [ .row.display create line \
			   32 46 46 46 \
			   -width $line_width ];

    # the motor displays
    # A
    set lcd_item(58) [ .row.display create polygon \
			   24 67 35 83 46 67 42 67 35 77 28 67 ];
    set lcd_item(30) [ .row.display create polygon \
			   9 78 17 72 17 83 ];
    set lcd_item(26) [ .row.display create polygon \
			   60 78 52 72 52 83 ];
    # B
    set lcd_item(6)  [ .row.display create polygon \
			   115 67 126 83 137 67 133 67 126 77 119 67 ];
    set lcd_item(10) [ .row.display create polygon \
			   100 78 108 72 108 83 ];
    set lcd_item(34) [ .row.display create polygon \
			   151 78 143 72 143 83 ];
    # C
    set lcd_item(14) [ .row.display create polygon \
			  205 67 216 83 227 67 223 67 216 77 209 67 ];
    set lcd_item(38) [ .row.display create polygon \
			   190 78 198 72 198 83 ];
    set lcd_item(22) [ .row.display create polygon \
			   241 78 233 72 233 83 ];

    # the sensor displays
    # 1
    set lcd_item(49) [ .row.display create polygon \
			   30 23 35 16 40 23 ];
    set lcd_item(48) [ .row.display create polygon \
			   24 23 35  8 46 23 42 23 35 14 28 23 ];
    # 2
    set lcd_item(40) [ .row.display create polygon \
			  121 23 126 16 131 23 ];
    set lcd_item(44) [ .row.display create polygon \
			  115 23 126  8 137 23 133 23 126 14 119 23 ];
    # 3
    set lcd_item(20) [ .row.display create polygon \
			  211 23 216 16 221 23 ];
    set lcd_item(36) [ .row.display create polygon \
			  205 23 216  8 227 23 223 23 216 14 209 23 ];

    # The quartered circle
    set lcd_item(16) [ .row.display create arc 220 33 244 57 \
			   -outline {} -start 0 -extent 90 ];
    set lcd_item(17) [ .row.display create arc 219 33 243 57 \
			   -outline {} -start 90 -extent 90 ];
    set lcd_item(19) [ .row.display create arc 219 34 243 58 \
			   -outline {} -start 180 -extent 90 ];
    set lcd_item(18) [ .row.display create arc 220 34 244 58 \
			   -outline {} -start 270 -extent 90 ];

    # The dotted line
    set lcd_item(52) [ .row.display create rectangle  53 12  60 19 -outline {} ];
    set lcd_item(64) [ .row.display create rectangle  65 12  72 19 -outline {} ];
    set lcd_item(68) [ .row.display create rectangle  77 12  84 19 -outline {} ];
    set lcd_item(56) [ .row.display create rectangle  89 12  96 19 -outline {} ];
    set lcd_item(60) [ .row.display create rectangle 101 12 108 19 -outline {} ];

    # IR Display
    set lcd_item(50) [ .row.display create polygon \
			   9 30 20 69 31 30 30 30 25 47 15 47 10 30 9 30 ];
    set lcd_item(51) [ .row.display create polygon \
			   11 30 20 29 29 30 24 46 16 46 ];

    # Battery
    set lcd_item(32) [ .row.display create polygon \
			   147 10 147 23 193 23 \
			   193 19 197 19 197 13 193 13 193 15 195 15 195 17 \
			   193 17 193 10 147 10 \
			   149 12 191 12 191 21 149 21 149 12 147 10 \
			   161  9 176 24 178 24 163  9 161  9 \
			   161 24 176  9 178  9 163 24 161 24 161 9 ];


    for {set i 0} { $i < 9} {incr i 1} { update_lcd $i 0; }
    pack .row.display -side left
}

proc send_sensor { sens val } {
    send_cmd [format "A$sens%03x" $val ];    
}
proc update_sensors { val } {
    for {set i 0} { $i < 3} {incr i 1} {
	if { $val & (1 << $i) } {
	    .sensors.$i configure -bg red
	} else {
	    .sensors.$i configure -bg yellow
	}
    }
    update;
}    
proc create_one_sensor { sens label } {
    scale .sensors.$sens -bg yellow -orient horizontal -showvalue 1 \
	-label $label -sliderlength 5 -width 15 -from 0 -to 1023 \
	-relief solid -borderwidth 0 \
	-command "send_sensor $sens";
    pack .sensors.$sens -side left -fill x
}

proc create_sensors { } {
    frame .sensors -bg yellow
    pack .sensors -fill x
    create_one_sensor 2 "Sensor 1"
    create_one_sensor 1 "Sensor 2"
    create_one_sensor 0 "Sensor 3"
    create_one_sensor 3 "Battery"
}

proc update_motor { mtr val promille } {
    if { $promille == 0 } {
	set val 0
    }
    
    if { $val == 2 } {
	.motors.$mtr.dir configure -text "-->" 
    } elseif { $val == 1 } {
	.motors.$mtr.dir configure -text "<--"
    } elseif { $val == 3 } {
	.motors.$mtr.dir configure -text "<->"
    } else {
	.motors.$mtr.dir configure -text ""
    }

    .motors.$mtr.speed configure -text $promille
}

proc create_one_motor { mtr label } {
    frame .motors.$mtr -relief raised -borderwidth 3
    label .motors.$mtr.dir -height 2 -width 9
    pack .motors.$mtr.dir
    label .motors.$mtr.speed -text "0" -height 2 -width 9
    pack .motors.$mtr.speed
    pack .motors.$mtr -side left
}

proc create_motors { } {
    frame .motors -bg yellow
    pack .motors -fill x
    create_one_motor 0 "A"
    create_one_motor 1 "B"
    create_one_motor 2 "C"
}

proc press_button_3 { button id} {
    if { [string equal [$button cget -relief ] "sunken" ]} {
	$button configure -relief raised
	send_cmd "B${id}0"
    } else {
	$button configure -relief sunken
	send_cmd "B${id}1" 
    }
}
proc create_button { frame side text id abg bg fg } {
    button $frame.btn$id -text $text -borderwidth 5 -activebackground $abg -bg $bg -activeforeground $fg -fg $fg -width 3 -height 1
    pack $frame.btn$id -side $side

    bind $frame.btn$id  <ButtonPress-1>   "send_cmd {B${id}1}"
    bind $frame.btn$id  <ButtonRelease-1> "send_cmd {B${id}0}"
    bind $frame.btn$id  <ButtonPress-3>   "press_button_3 $frame.btn$id $id"
}

proc save_savefile { } {
    set types {
	{{Brick Save File} {.bsf} }
	{{All Files} *  }
    }
    set filename [ tk_getSaveFile -defaultextension ".bsf" -filetypes $types ]
    if {$filename != ""} {
	send_cmd "CS$filename"
    }
}

proc load_savefile { } {
    set types {
	{{Brick Save File} {.bsf} }
	{{All Files} *  }
    }
    set filename [ tk_getOpenFile -filetypes $types ]
    if {$filename != ""} {
	send_cmd "CL$filename"
    }
}

proc beam_firmware { filename } {
    send_cmd "PR"
    after 100 "send_cmd {BO1}; after 100 {send_cmd {BO0}; after 100 {send_cmd {F$filename}; send_cmd {OO}}}"
}

proc reset { } {
    global firmware
    if {$firmware != ""} {
	beam_firmware $firmware;
    } else {
	send_cmd "PR"
	send_cmd "OO"
    }
}

proc debug { } {
    global firmware progs CROSSTOOLPREFIX brickaddr debuggerport
    set initfd [ open ".gdbinit" [list CREAT TRUNC WRONLY]  ] 
    send_cmd "PD"
    vwait debuggerport
    puts $initfd "target remote localhost:$debuggerport"
    if { $firmware != "" } {
	puts $initfd "add-symbol-file $firmware 0x8000"
    }
#    for { set i 0 } { $i < 7 } { incr i 1 } {
#	if {[ lindex $progs $i ] != ""} {
#	    puts $initfd "add-symbol-file [lindex $progs $i]"
#	}
#    }
#    puts $initfd "break main"
    close $initfd
    exec ddd --gdb --debugger ${CROSSTOOLPREFIX}gdb [lindex $progs 1] &
}

proc load_firmware { } {
    global firmware
    set types {
	{{Firmware} {.coff .srec .lgo} }
	{{All Files} *  }
    }
    set filename [ tk_getOpenFile -filetypes $types ]
    if {$filename != ""} { 
	set firmware $filename
	beam_firmware $firmware
    }
}

proc load_program { nr } {
    set types {
	{{Archive} {.a} }
	{{Relocatable Coff} {.rcoff} }
	{{lx} {.lx} }
	{{All Files} *  }
    }
    set filename [ tk_getOpenFile -filetypes $types ]
    if {[string match "*.rcoff" $filename]} {
	global firmware brickaddr CROSSTOOLPREFIX
	global progs
	set coffname [string map {.rcoff .coff} $filename]
	set firmlds  [string map {.coff .lds} $firmware]
	set sizefd [open "|${CROSSTOOLPREFIX}size $filename" "r" ]
	gets $sizefd line
	gets $sizefd line
	scan $line " %d %d %d" text data bss
	set size [expr $text + $data + $bss + $data]
	send_cmd [format "OA%1d%d" [expr $nr - 1] $size]
	vwait brickaddr
	regexp {[^/]*$} $filename basename
	puts "$basename loaded to $brickaddr"
	if { $brickaddr } {
	    exec ${CROSSTOOLPREFIX}ld -T $firmlds $filename -o $coffname --oformat coff-h8300 -Ttext $brickaddr
	    set filename $coffname
	    lset progs $nr $coffname
	} else {
	    set filename ""
	}
    } elseif {[string match "*.a" $filename]} {
	global firmware brickaddr CROSSTOOLPREFIX
	global progs
	global BRICKOS_LIBDIR LIBS
	set coffname [string map {.a .coff} $filename]
	set objname  [string map {.a .o} $filename]
	set firmlds  [string map {.coff .lds} $firmware]
	exec ${CROSSTOOLPREFIX}ld -T $firmlds -L$BRICKOS_LIBDIR $objname $filename -lc -lmint -lfloat -lc++ -o $coffname --oformat coff-h8300 -Ttext 0xb000
	set sizefd [open "|${CROSSTOOLPREFIX}size $coffname" "r" ]
	gets $sizefd line
	gets $sizefd line
	scan $line " %d %d %d" text data bss
	set size [expr $text + $data + $bss + $data]
	send_cmd [format "OA%1d%d" [expr $nr - 1] $size]
	vwait brickaddr
	regexp {[^/]*$} $filename basename
	puts "$basename loaded to $brickaddr"
	if { $brickaddr } {
	    exec ${CROSSTOOLPREFIX}ld -T $firmlds -L$BRICKOS_LIBDIR $objname $filename -lc -lmint -lfloat -lc++ -o $coffname --oformat coff-h8300 -Ttext $brickaddr
	    set filename $coffname
	    lset progs $nr $coffname
	} else {
	    set filename ""
	}
    }
    if {$filename != ""} {
	send_cmd [format "OL%1d%s" [expr $nr - 1] $filename ]
    }
}

set bosaddr 0
proc update_brickos { cmd } {
    global bosaddr
    
    switch [string index $cmd 1] {
	N { if [scan $cmd "ON%x" addr] {
	        set bosaddr $addr
	    }
	}
	O { scan $cmd "OO%d" status
	    if $status {
		.mainmenu entryconfigure 2 -state normal
	    } else {
		.mainmenu entryconfigure 2 -state disable
	    }
	}
	A { 
	    global brickaddr
	    scan $cmd "OA%x" addr
	    set brickaddr [format "0x%x" $addr]
	}
    }
}
proc bos_setaddr { } {
    global bosaddr
    send_cmd [format "ON%x" $bosaddr]
    send_cmd "ON?"
}

proc create_bosmenus { } {
    global bosaddr
    menu .bosaddrmenu -postcommand {send_cmd "ON?"}
    .bosaddrmenu add radiobutton -label "0" -variable bosaddr -command { bos_setaddr }
    .bosaddrmenu add radiobutton -label "1" -variable bosaddr -command { send_cmd "ON1" }
    .bosaddrmenu add radiobutton -label "2" -variable bosaddr -command { send_cmd "ON2" }
    .bosaddrmenu add radiobutton -label "3" -variable bosaddr -command { send_cmd "ON3" }
    .bosaddrmenu add radiobutton -label "4" -variable bosaddr -command { send_cmd "ON4" }
    .bosaddrmenu add radiobutton -label "5" -variable bosaddr -command { send_cmd "ON5" }
    .bosaddrmenu add radiobutton -label "6" -variable bosaddr -command { send_cmd "ON6" }
    .bosaddrmenu add radiobutton -label "7" -variable bosaddr -command { send_cmd "ON7" }
    .bosaddrmenu add radiobutton -label "8" -variable bosaddr -command { send_cmd "ON8" }
    .bosaddrmenu add radiobutton -label "9" -variable bosaddr -command { send_cmd "ON9" }
    .bosaddrmenu add radiobutton -label "10" -variable bosaddr -command { send_cmd "ONa" }
    .bosaddrmenu add radiobutton -label "11" -variable bosaddr -command { send_cmd "ONb" }
    .bosaddrmenu add radiobutton -label "12" -variable bosaddr -command { send_cmd "ONc" }
    .bosaddrmenu add radiobutton -label "13" -variable bosaddr -command { send_cmd "ONd" }
    .bosaddrmenu add radiobutton -label "14" -variable bosaddr -command { send_cmd "ONe" }
    .bosaddrmenu add radiobutton -label "15" -variable bosaddr -command { send_cmd "ONf" }

    menu .bosprogmenu
    .bosprogmenu add command -label "1..." -command {load_program 1}
    .bosprogmenu add command -label "2..." -command {load_program 2}
    .bosprogmenu add command -label "3..." -command {load_program 3}
    .bosprogmenu add command -label "4..." -command {load_program 4}
    .bosprogmenu add command -label "5..." -command {load_program 5}
    .bosprogmenu add command -label "6..." -command {load_program 6}
    .bosprogmenu add command -label "7..." -command {load_program 7}
}

proc show_about_box { } {
    tk_messageBox -icon info -title "About BrickEMU" -message "BrickEmu (C) 2003-2004 Jochen Hoenicke\n\nThis program is free software; you can redistribute it and/or modify it under the terms of the GPL.\n\nYou can find the latest version at:\nhttp://hoenicke.ath.cx/rcx/"
}

proc create_menu { } {
    create_bosmenus
    menu .filemenu
    .filemenu add command -label "Reset" -command { reset }
    .filemenu add command -label "Open..." -command {load_savefile}
    .filemenu add command -label "Save As..." -command {save_savefile}
    .filemenu add command -label "Firmware..." -command {load_firmware}
    .filemenu add separator
    .filemenu add command -label "Debug" -command { debug }
    .filemenu add separator
    .filemenu add command -label "Exit" -command {exit}

    menu .brickosmenu
    .brickosmenu add cascade -label "Load Program" -menu .bosprogmenu
    .brickosmenu add cascade -label "Set Network Address" -menu .bosaddrmenu
    .brickosmenu add separator
    .brickosmenu add command -label "Dump memory" -command {send_cmd "OM"}
    .brickosmenu add command -label "Dump threads" -command {send_cmd "OT"}
    .brickosmenu add command -label "Dump timers" -command {send_cmd "OC"}
    .brickosmenu add command -label "Dump programs" -command {send_cmd "OP"}

    menu .helpmenu
    .helpmenu add command -label "About..." -command {show_about_box}

    menu .mainmenu
    .mainmenu add cascade -label "File" -menu .filemenu
    .mainmenu add cascade -label "BrickOS" -menu .brickosmenu
    .mainmenu add cascade -label "Help" -menu .helpmenu
    . configure -menu .mainmenu
}

proc create_gui { } {
    wm title . "BrickEmu"
    create_menu
    create_sensors
    . configure -bg yellow;
    frame .row -bg yellow
    pack .row -pady 10 -padx 6;
    frame .row.left -bg yellow;
    pack .row.left -fill y -side left -padx 6 -pady 1;
    create_button .row.left top    "View"   V gray40   black white
    create_button .row.left bottom "On-Off" O DeepPink red   black

    create_display;

    frame .row.right -bg yellow;
    pack .row.right -fill y -side left -padx 6 -pady 1;
    create_button .row.right top   Prgm P grey90 grey80 black
    create_button .row.right bottom Run R MediumTurquoise LightSeaGreen black
    create_motors
    tkwait visibility .row.display
}


proc startit { } {
    global emufd;

    fileevent $emufd readable { 
	global emufd;
	gets $emufd cmd;
	#puts "reply: $cmd"
	if {[string length $cmd] == 0} { puts "Stream closed!"; exit }

	switch [string index $cmd 0] {
	    L { scan $cmd "L%d,%x" idx val
		update_lcd $idx $val
	    }
	    A { scan $cmd "A%d" val
		update_sensors $val
	    }
	    M { scan $cmd "M%d,%d,%d" mtr val promille
		update_motor $mtr $val $promille
	    }
	    O { 
		update_brickos $cmd
	    }
	    P { global debuggerport
		scan $cmd "PD%d" addr
		set debuggerport $addr
	    }
	    default { puts "GUI: unknown command: '$cmd'";}
	}
    }

    .sensors.2 set 1023
    .sensors.1 set 1023
    .sensors.0 set 1023
    .sensors.3 set 320

    send_cmd "OO"
}

#---------------------------------
#      HAUPTPROGRAMM
#---------------------------------

set portnr 0
for { set i 0 } { $i < $argc } {incr i 1} {
    if { [lindex $argv $i] == "-firm" } {
	incr i 1
	set firmware [lindex $argv $i]
    } else {
	set portnr [lindex $argv $i]
    }
}

if { $portnr == 0 } {

    if { [llength [array get env BRICKEMU_DIR]] != 0 } {
	set brickemu_dir $env(BRICKEMU_DIR)
    } else {
	set brickemu_dir "."
    }

    proc start_server { fd addr port } {
	global emufd firmware;
	set emufd $fd
	
	puts "Starting GUI..."
	startit;
	if {$firmware != ""} { after 200 "beam_firmware $firmware" }
    }
	
    create_gui
    set fd [socket -server start_server 0]
    set portnr [lindex [fconfigure $fd -sockname] 2]
    puts "Starting: $brickemu_dir/emu $portnr"
    exec $brickemu_dir/emu -serverport $portnr &
} else {
    global emufd;

    if [catch {
	set emufd [ socket localhost $portnr ];
    } msg ] { puts $msg ; exit }

    create_gui
    startit;
    if {$firmware != ""} { after 200 "beam_firmware $firmware" }
}
