# Emulator for LEGO RCX Brick, Copyright (C) 2003 Jochen Hoenicke.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; see the file COPYING.LESSER.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
#
# $Id: h8300.pl 314 2006-03-16 18:25:26Z hoenicke $
#


sub getWRo() {
    "   uint16 oval = GET_REG16(opcval & 0x7);\n";
}
sub getWRd() {
    "   uint16 dest = GET_REG16(opcval & 0x7);\n";
}
sub getWRs() {
    "   uint16 src  = GET_REG16((opcval>>4) & 0x7);\n";
}
sub getBRo() {
    "   uint8 oval = reg[opcval & 0xf];\n";
}
sub getBRd() {
    "   uint8 dest = reg[opcval & 0xf];\n";
}
sub getBRs() {
    "   uint8 src  = reg[(opcval>>4) & 0xf];\n";
}

sub setNZ($) {
    my $w = $_[0] =~ /W$/ ? 16 : 8;
    return
	"   ccr |= (dest >> ".($w - 4).") & 0x08;\n".
	"   if (dest == 0)  ccr |= 0x04;\n";
}

sub setHNZVC($) {
    my $w = $_[0] =~ /W$/ ? 16 : 8;
    my $res;

    $auxmask = (1 << ($w-4)) - 1;
    $res = 
	"   ccr &= 0xd0;\n".
	setNZ($_[0]);
    if ($_[0] =~ /^SUB/) {
	$res .= 
	    "   ccr |= (((src ^ oval) & (dest ^ oval)) >> ($w-2)) & 0x02;\n".
	    "   ccr |= ((src ^ dest ^ oval) ".($w==16 ? ">> 7": "<< 1").") & 0x20;\n".
	    "   ccr |= ((src ^ ((src ^ dest) & (dest ^ oval))) >> ($w-1)) & 0x01;\n";
    } else {
	$res .= 
	    "   ccr |= (((src ^ dest) & (dest ^ oval)) >> ($w-2)) & 0x02;\n".
	    "   ccr |= ((src ^ dest ^ oval) ".($w==16 ? ">> 7": "<< 1").") & 0x20;\n".
	    "   ccr |= ((src ^ ((src ^ oval) & (dest ^ oval))) >> ($w-1)) & 0x01;\n";
    }
    return $res;
}

sub setNZV($) {
    my $w = $_[0] =~ /W$/ ? 16 : 8;
    return "   ccr &= 0xf1;\n".
	setNZ($_[0]);
}


sub setWRd() {
    "   SET_REG16(opcval & 0x7, dest);\n";
}
sub setBRd() {
    "   reg[opcval & 0xf] = dest;\n";
}

sub call($) {
    return
	"  {uint16 sp = GET_REG16(7) - 2;\n".
	"   WRITE_WORD(sp, pc);\n".
	"   pc = $_[0];\n".
	check_pc().
	"   frame_begin(sp, 0);\n".
        "   SET_REG16(7, sp);}\n";
}


########  Misc instructions ###########################

sub Nop() {
    return
	"   if (opcval == 0 && GET_WORD(pc) == 0x5470 /* rts*/)\n".
	"     debug_printf();\n";
}

sub Sleep() {
    "   cpu_sleep();\n";
}

sub Trap() {
    "   db_trap = TRAP_EXCEPTION;\n";
}

######### Arithmetic instructions ########################

sub AddBI() {
    "   uint8 nr = (opc >> 8) & 0xf;\n".
	"   uint8 oval = reg[nr];\n".
	"   uint8 src = opcval;\n".
	"   uint8 dest = oval + src;\n".
	setHNZVC("ADDB"). 
	"   reg[nr] = dest;\n";
}
sub AddB() {
    return
	getBRo(). 
	getBRs().
	"   uint8 dest = oval + src;\n".
	setHNZVC("ADDB"). 
        setBRd();
}
sub AddW() {
    getWRo(). 
	getWRs().
	"   if (opcval & 0x0088) goto illOpc;\n". 
	"   uint16 dest = oval + src;\n".
	setHNZVC("ADDW"). 
        setWRd();
}
sub AddS() {    
    return
	"   int8 nr = opcval & 0x7;\n".
	"   if (opcval & 0x0078) goto illOpc;\n". 
	"   SET_REG16(nr, GET_REG16(nr) + (opcval >> 7) + 1);\n";
}
sub AddXI() {
    return 
	"   uint8 nr = (opc >> 8) & 0xf;\n".
	"   uint8 oval = reg[nr];\n".
	"   uint8 src = opcval;\n".
	"   uint8 dest = oval + src + (ccr & 1);\n".
	setHNZVC("ADDX"). 
	"   reg[nr] = dest;\n";
}

sub AddX() {
    return
	getBRo(). 
	getBRs().
	" uint8 dest = oval + src + (ccr & 1);\n".
	setHNZVC("ADDX"). 
        setBRd();
}

sub CmpBI() {
    "   uint8 oval = reg[(opc >> 8) & 0xf];\n".
	"   uint8 src = opcval;\n".
	"   uint8 dest = oval - src;\n".
	setHNZVC("SUBB");
}
sub CmpB() {
    return
	getBRo(). 
	getBRs().
	"   uint8 dest = oval - src;\n".
	setHNZVC("SUBB");
}
sub CmpW() {
    getWRo(). 
	getWRs().
	"   if (opcval & 0x0088) goto illOpc;\n". 
	"   uint16 dest = oval - src;\n".
	setHNZVC("SUBW");
}

sub SubB() {
    return
	getBRo(). 
	getBRs().
	"   uint8 dest = oval - src;\n".
	setHNZVC("SUBB"). 
        setBRd();
}
sub SubW() {
    getWRo(). 
	getWRs().
	"   if (opcval & 0x0088) goto illOpc;\n". 
	"   uint16 dest = oval - src;\n".
	setHNZVC("SUBW"). 
        setWRd();
}
sub SubS() {
    return 
	"   int8 nr = opcval & 0x7;\n".
	"   if (opcval & 0x0078) goto illOpc;\n". 
	"   SET_REG16(nr, GET_REG16(nr) - (opcval >> 7) - 1);\n";
}

sub SubXI() {
    return 
	" uint8 nr = (opc >> 8) & 0xf;\n".
	" uint8 oval = reg[nr];\n".
	" uint8 src = opcval;\n".
	" uint8 dest = oval - src - (ccr & 1);\n".
	" uint8 pccr = ccr;\n".
	setHNZVC("SUBX"). 
	" reg[nr] = dest;\n".
        " ccr &= pccr | ~4;\n";
}

sub SubX() {
    return
	getBRo(). 
	getBRs().
	" uint8 pccr = ccr;\n".
	" uint8 dest = oval - src - (ccr & 1);\n".
	setHNZVC("SUBX"). 
        setBRd().
        " ccr &= pccr | ~4;\n";
}

sub Inc() {
    getBRd().
	"   if (opcval & 0xf0) goto illOpc;\n". 
	"   dest++;\n".
	setNZV("INC"). 
	"   if (dest == 0x80) ccr |= 0x2;\n".
        setBRd();
}
sub Dec() {
    getBRd().
	"   if (opcval & 0xf0) goto illOpc;\n". 
	"   dest--;\n".
	setNZV("DEC"). 
	"   if (dest == 0x7f) ccr |= 0x2;\n".
        setBRd();
	
}


sub DAA() {
    return
	getBRd().
	"   ccr &= 0xd1;\n".
	"   if ((ccr & 0x20) || (dest & 0x0f) > 0x09) dest += 6;\n".
	"   if ((ccr & 0x01) || (dest & 0xf0) > 0x90) {\n".
	"      dest += 0x60;\n".
	"      ccr |= 1;\n".
	"   }\n".
	setNZV("DAA").
        setBRd();
}
sub DAS() {
    return
	getBRd().
	"   ccr &= 0xd1;\n".
	"   if (ccr & 0x20) dest += 0xfa;\n".
	"   if (ccr & 0x01) dest += 0xa0;\n".
	setNZV("DAA").
        setBRd();
}
sub MulXU() {
    return  "   uint16 dest = reg[(opcval & 7) + 8];\n".
	getBRs().
	"   if (opcval & 0x08) goto illOpc;\n".
	"   dest = dest * src;\n".
	"   cycles += 6;\n".
	setWRd();
}
sub DivXU() {
    return getWRd(). getBRs() .
	"   if (opcval & 0x08) goto illOpc;\n".
	"   ccr &= 0xf3;\n".
	"   ccr |= ((src >> 4) & 0x8);\n".
	"   if (src != 0)\n".
	"      dest = ((dest / src) & 0xff) + ((dest % src) << 8);\n".
	"   else ccr |= 0x4;\n".
	"   cycles += 6;\n".
	setWRd();
}


######### Bit instructions ########################

my @bitops = 
    ( [ [0x63, "BTst", &BTst],
	[0x73, "BTstI", &BTstI],
	[0x74, "BOr", &BOr],
	[0x75, "BXor", &BXor],
	[0x76, "BAnd", &BAnd],
	[0x77, "BLd", &BLd] ],
      [ [0x60, "BSet", &BSet],
	[0x61, "BNot", &BNot],
	[0x62, "BClr", &BClr],
	[0x67, "BSt", &BSt],
	[0x70, "BSetI", &BSetI],
	[0x71, "BNotI", &BNotI],
	[0x72, "BClrI", &BClrI] ] );

	   
sub BitAbs() {
    my $nr = $_[0] & 1;
    my $res = 
	"  uint16 addr = (uint16) (int8) opcval;\n".
	"  uint8 val = READ_BYTE(addr);\n".
	"  GET_OPCODE;\n".
	"  opcval = opc & 0xff;\n".
	"  switch(opc >> 8) {\n";
    for (@{$bitops[$nr]}) {
	$code = $_->[2];
	$code =~ s/reg\[opcval & 0xf\]/val/g;
	$res .= "  case 0x".sprintf("%02x", $_->[0]). ": /* $_->[1] */\n";
	$res .= "  {\n$code   break;\n  }\n";
    }
    $res .= "  default: goto illOpc;\n";
    $res .= "  }\n";
    $res .= "  WRITE_BYTE(addr, val);\n" if ($nr);
    return $res;
}
	
sub BitInd() {
    my $nr = $_[0] & 1;
    my $res = 
	"  if (opcval & 0x8f) goto illOpc;\n".
	" {uint16 addr = GET_REG16(opcval >> 4);\n".
	"  uint8 val = READ_BYTE(addr);\n".
	"  GET_OPCODE;\n".
	"  opcval = opc & 0xff;\n".
	"  if (opcval & 0x0f) goto illOpc;\n".
	"  switch(opc >> 8) {\n";
    for (@{$bitops[$nr]}) {
	$code = $_->[2];
	$code =~ s/reg\[opcval & 0xf\]/val/g;
	$res .= "  case 0x".sprintf("%02x", $_->[0]). ": /* $_->[1] */\n";
	$res .= "  {\n$code   break;\n  }\n";
    }
    $res .= "  default: goto illOpc;\n";
    $res .= "  }\n";
    $res .= "  WRITE_BYTE(addr, val);\n" if ($nr);
    $res .= "  }\n";
    return $res;
}	

sub BAnd() {
    return
	"   ccr &= 0xfe | ( (reg[opcval & 0xf] >> ((opcval >> 4) & 7))\n".
	"                       ^ (opcval >> 7));\n";
}
sub BLd() {
    return
	"   ccr &= 0xfe;\n".
	"   ccr |= 0x1 & ( (reg[opcval & 0xf] >> ((opcval >> 4) & 7))\n".
	"                      ^ (opcval >> 7));\n";
}
sub BSt() {
    return
	"   if ((ccr ^ (opcval >> 7)) & 1)\n".
	"      reg[opcval & 0xf] |= (1 << ((opcval >> 4) & 7));\n".
        "   else\n".
	"      reg[opcval & 0xf] &= ~(1 << ((opcval >> 4) & 7));\n";
}


sub BOr() {
    return
	"   ccr |= 0x1 & ( (reg[opcval & 0xf] >> ((opcval >> 4) & 7))\n".
	"                      ^ (opcval >> 7));\n";
}
sub BXor() {
    return
	"   ccr ^= 0x1 & ( (reg[opcval & 0xf] >> ((opcval >> 4) & 7))\n".
	"                      ^ (opcval >> 7));\n";
}


sub BClrI() {
    return 
	"   if (opcval & 0x80) goto illOpc;\n".
	"   reg[opcval & 0xf] &= ~(1 << (opcval >> 4));\n";
}

sub BClr() {
    return 
	"   reg[opcval & 0xf] &= ~(1 << (reg[opcval >> 4] & 7));\n";
}

sub BSetI() {
    return 
	"   if (opcval & 0x80) goto illOpc;\n".
	"   reg[opcval & 0xf] |= (1 << (opcval >> 4));\n";
}

sub BSet() {
    return 
	"   reg[opcval & 0xf] |= (1 << (reg[opcval >> 4] & 7));\n";
}

sub BNotI() {
    return 
	"   if (opcval & 0x80) goto illOpc;\n".
	"   reg[opcval & 0xf] ^= (1 << (opcval >> 4));\n";
}
sub BNot() {
    return 
	"   reg[opcval & 0xf] ^= (1 << (reg[opcval >> 4] & 7));\n";
}
sub BTstI() {
    return 
	"   if (opcval & 0x80) goto illOpc;\n".
	"   ccr &= 0xfb;\n".
	"   ccr |= (0x1 & (~reg[opcval & 0xf] >> (opcval >> 4)))<<2;\n";
}
sub BTst() {
    return 
	"   ccr &= 0xfb;\n".
	"   ccr |= (0x1 & (~reg[opcval & 0xf] >> (reg[opcval >> 4] & 7)))<<2;\n";
}

######### Branch instructions ########################

sub check_pc() {
    return
	"   if ((pc & 1) || (memtype[pc] & MEMTYPE_DIV))\n".
	"      { db_trap = 10; goto fault;  }\n";
}

sub BRA() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"  pc += (int8) opcval;\n".
	check_pc();
}
sub BRN() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"";
}
sub BHI() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"  if (!(ccr & 0x5)) pc += (int8) opcval;\n".
	check_pc();
}
sub BLO() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"  if ((ccr & 0x5)) pc += (int8) opcval;\n".
	check_pc();
}
sub BCC() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"  if (!(ccr & 0x1)) pc += (int8) opcval;\n".
	check_pc();
}
sub BCS() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"  if ((ccr & 0x1)) pc += (int8) opcval;\n".
	check_pc();
}
sub BNE() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"  if (!(ccr & 0x4)) pc += (int8) opcval;\n".
	check_pc();
}
sub BEQ() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"  if ((ccr & 0x4)) pc += (int8) opcval;\n".
	check_pc();
}
sub BVC() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"  if (!(ccr & 0x2)) pc += (int8) opcval;\n".
	check_pc();
}
sub BVS() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"  if ((ccr & 0x2)) pc += (int8) opcval;\n".
	check_pc();
}
sub BPL() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"  if (!(ccr & 0x8)) pc += (int8) opcval;\n".
	check_pc();
}
sub BMI() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"  if ((ccr & 0x8)) pc += (int8) opcval;\n".
	check_pc();
}
sub BGE() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"  if (!(((ccr >> 2) ^ ccr) & 0x2)) pc += (int8) opcval;\n".
	check_pc();
}
sub BLT() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"  if (((ccr >> 2) ^ ccr) & 0x2) pc += (int8) opcval;\n".
	check_pc();
}
sub BGT() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"  if (!((((ccr >> 2)&2) ^ ccr) & 0x6)) pc += (int8) opcval;\n".
	check_pc();
}
sub BLE() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"  if ((((ccr >> 2)&2) ^ ccr) & 0x6) pc += (int8) opcval;\n".
	check_pc();
}

sub BSr() {
    return "   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	call("pc + (int8) opcval");
}

sub JmpRI() {
    return
	"   if (opcval & 0x8f) goto illOpc;\n".
	"   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"   pc = GET_REG16(opcval >> 4);\n".
	check_pc();
}
sub JmpA16() {
    return
	"   if (opcval) goto illOpc;\n".
	"   cycles += 2;\n".
	"   pc = GET_WORD_CYCLES(pc);\n".
	check_pc();
}
sub JmpAA8() {
    return
	"   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"   cycles += 2;\n".
	"   pc = READ_WORD((uint16)(int8) opcval);\n".
	check_pc();
}
sub JsrRI() {
    return
	"   if (opcval & 0x8f) goto illOpc;\n".
	"   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	call("GET_REG16(opcval >> 4)");
}
sub JsrA16() {
    return "   uint16 addr;\n".
	"   if (opcval) goto illOpc;\n".
	"   addr = GET_WORD_CYCLES(pc); pc += 2;\n".
	"   cycles += 2;\n".
	call("addr");
}
sub JsrAA8() {
    return
	"   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	call("READ_WORD((uint16)(int8) opcval)");
}

sub RtS() {
    return 
	"   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"   cycles += 2;\n".

	"  {uint16 fp = GET_REG16(7);\n".
	"   frame_end(fp, 0);\n".
	"   pc = READ_WORD(fp);\n".
	check_pc().
        "   SET_REG16(7, fp + 2);}\n";
}
sub RtE() {
    return 
	"   GET_WORD_CYCLES(pc); /* emulate prefetch */\n".
	"   cycles += 2;\n".
	"  {uint16 fp = GET_REG16(7);\n".
	"   uint16 ccrword;\n".
	"   frame_end(fp+2, 1);\n".
	"   ccrword = READ_WORD(fp); ccr = ccrword>>8;\n".
	"   pc  = READ_WORD(fp+2);\n".
	check_pc().
        "   SET_REG16(7, fp + 4);}\n";
}
    
	


######### Move instructions ########################

sub EepMov() {
    return "   implement(\"EEPMOV\");\n";
}

sub MovB() {
    return "   uint8 dest  = reg[(opcval>>4) & 0xf];\n".
	setNZV("MOVB"). 
        setBRd();
}
sub MovBI() {
    return
	"   uint8 dest  = opcval;\n".
	setNZV("MOVB"). 
	"   reg[(opc >> 8) & 0xf] = dest;\n";
}
sub MovBRI() {
    return "   uint8 dest;\n".
	"   if (opcval & 0x80) {\n". 
        "      dest = reg[opcval & 0xf];\n".
	"      WRITE_BYTE(GET_REG16((opcval >> 4) & 0x7), dest);\n".
        "   } else {\n".
	"      dest = READ_BYTE(GET_REG16(opcval >> 4));\n".
        "   ".setBRd().
	"   }\n".
	setNZV("MOVB");
}
sub MovBRID() {
    return "   uint8 dest;\n".
	"   uint16 addr;\n".
	"   addr = GET_WORD_CYCLES(pc) + GET_REG16((opcval >> 4) & 0x7); pc +=2;\n".
	"   if (opcval & 0x80) {\n". 
        "      dest = reg[opcval & 0xf];\n".
	"      WRITE_BYTE(addr, dest);\n".
        "   } else {\n".
	"      dest = READ_BYTE(addr);\n".
        "   ".setBRd().
	"   }\n".
	setNZV("MOVB");
}
sub MovBRIP() {
    return " uint8 dest;\n".
	"   cycles += 2;\n".
	"   if (opcval & 0x80) {\n". 
	"      uint16 addr = GET_REG16((opcval >> 4) & 0x7) - 1;\n".
        "      dest = reg[opcval & 0xf];\n".
	"      WRITE_BYTE(addr, dest);\n".
        "      SET_REG16((opcval >> 4) & 0x7, addr);\n".
        "   } else {\n".
	"      uint16 addr = GET_REG16(opcval >> 4);\n".
	"      dest = READ_BYTE(addr);\n".	
        "      SET_REG16(opcval >> 4, addr + 1);\n".
        "   ".setBRd().
        " }\n".
	setNZV("MOVB");
}
sub MovBFA8() {
    return
	"   uint8 dest;\n".
	"   dest = READ_BYTE((uint16)(int8)opcval);\n".
	setNZV("MOVB"). 
        "   reg[(opc >> 8) & 0xf] = dest;\n";
}
sub MovBTA8() {
    return
	"   uint8 dest = reg[(opc >> 8) & 0xf];\n".
	setNZV("MOVB"). 
	"   WRITE_BYTE((uint16)(int8)opcval, dest);\n";
}
sub MovBA16() {
    return "   uint8 dest;\n".
	"   uint16 addr;\n".
	"   if (opcval & 0x70) goto illOpc;\n". 
	"   addr = GET_WORD_CYCLES(pc); pc +=2;\n".
	"   if (opcval & 0x80) {\n". 
        "      dest = reg[opcval & 0xf];\n".
	"      WRITE_BYTE(addr, dest);\n".
        "   } else {\n".
	"      dest = READ_BYTE(addr);\n".
        "   ".setBRd().
        " }\n".
	setNZV("MOVB");
}


sub MovW() {
    return "   uint16 dest  = GET_REG16((opcval>>4) & 0x7);\n".
	"   if (opcval & 0x0088) goto illOpc;\n". 
	setNZV("MOVW"). 
	"   if ((opcval & 0x7) == 7) frame_switch(GET_REG16(7), dest);\n".
        setWRd();
}
sub MovWI() {
    return "   uint16 dest;\n".
	"   if (opcval & 0xf8) goto illOpc;\n". 
	"   dest = GET_WORD_CYCLES(pc); pc += 2;\n".
	setNZV("MOVW"). 
	"   if ((opcval & 0x7) == 7) frame_switch(GET_REG16(7), dest);\n".
	setWRd();
}
sub MovWRI() {
    return "   uint16 dest;\n".
	"   if (opcval & 0x08) goto illOpc;\n". 
	"   if (opcval & 0x80) {\n". 
	"      dest = GET_REG16(opcval & 0x7);\n".
        "      WRITE_WORD(GET_REG16((opcval >> 4) & 0x7), dest);\n".
	"   } else {\n". 
	"      dest = READ_WORD(GET_REG16(opcval >> 4));\n".
	"      if ((opcval & 0x7) == 7) frame_switch(GET_REG16(7), dest);\n".
        "   ".setWRd().
	"   }\n". 
	setNZV("MOVW");
}
sub MovWRID() {
    return "   uint16 dest;\n".
	"   uint16 addr;\n".
	"   if (opcval & 0x08) goto illOpc;\n". 
	"   addr = GET_WORD_CYCLES(pc) + GET_REG16((opcval >> 4) & 0x7); pc +=2;\n".
	"   if (opcval & 0x80) {\n". 
	"      dest = GET_REG16(opcval & 0x7);\n".
        "      WRITE_WORD(addr, dest);\n".
	"   } else {\n". 
	"      dest = READ_WORD(addr);\n".
	"      if ((opcval & 0x7) == 7) frame_switch(GET_REG16(7), dest);\n".
        "   ".setWRd().
	"   }\n". 
	setNZV("MOVW");
}
sub MovWRIP() {
    return "   uint16 dest;\n".
	"   if (opcval & 0x08) goto illOpc;\n". 
	"   cycles += 2;\n".
	"   if (opcval & 0x80) {\n". 
	"      uint16 addr = GET_REG16((opcval >> 4) & 0x7) - 2;\n".
	"      dest = GET_REG16(opcval & 0x7);\n".
        "      WRITE_WORD(addr, dest);\n".
	"      SET_REG16((opcval >> 4) & 0x7, addr);\n".
	"   } else {\n". 
	"      uint16 addr = GET_REG16(opcval >> 4);\n".
	"      dest = READ_WORD(addr); SET_REG16(opcval >> 4, addr + 2);\n".
	"      if ((opcval & 0x7) == 7) frame_switch(GET_REG16(7), dest);\n".
        "   ".setWRd().
	"   }\n". 
	setNZV("MOVW");
}
sub MovWA16() {
    return "   uint16 dest;\n".
	"   uint16 addr;\n".
	"   if (opcval & 0x78) goto illOpc;\n". 
	"   addr = GET_WORD_CYCLES(pc); pc +=2;\n".
	"   if (opcval & 0x80) {\n". 
	"      dest = GET_REG16(opcval & 0x7);\n".
        "      WRITE_WORD(addr, dest);\n".
	"   } else {\n". 
	"      dest = READ_WORD(addr);\n".
	"      if ((opcval & 0x7) == 7) frame_switch(GET_REG16(7), dest);\n".
        "   ".setWRd().
	"   }\n". 
	setNZV("MOVW");
}


######### Flag instructions ########################
sub StC() {
    return "   if (opcval & 0xf0) goto illOpc;\n". 
	"   reg[opcval] = ccr;\n";
}
sub LdC() {
    return "   if (opcval & 0xf0) goto illOpc;\n". 
	"   ccr = reg[opcval];\n" .
	"   irq_disabled_one = 1;\n";
}
sub LdCI() {
    return "   ccr = opcval;\n".
	"   irq_disabled_one = 1;\n";
}
sub AndC() {
    return "   ccr &= opcval;\n".
	"   irq_disabled_one = 1;\n";

}
sub OrC() {
    return "   ccr |= opcval;\n".
	"   irq_disabled_one = 1;\n";

}
sub XorC() {
    return "   ccr ^= opcval;\n".
	"   irq_disabled_one = 1;\n";
}

######### Shift instructions ########################
sub ShL() {
    getBRd().
    "   if (opcval & 0x70) goto illOpc;\n".
    "   ccr &= 0xf0;".
    "   ccr |= dest >> 7;\n".
    "   dest <<= 1;\n".
    "   if (opcval & 0x80 && (((dest >> 7) ^ ccr) & 1)) ccr |= 0x2;\n".
    setBRd().
    setNZ("SHL");
}
sub ShR() {
    getBRd().
    "   if (opcval & 0x70) goto illOpc;\n".
    "   ccr &= 0xf0;".
    "   ccr |= dest & 1;\n".
    "   if (opcval & 0x80)\n".
    "      dest = ((int8)dest) >> 1;\n".
    "   else\n".
    "      dest >>= 1;\n".
    setBRd().
    setNZ("SHR");
}
sub RotL() {
    getBRd().
    "   if (opcval & 0x70) goto illOpc;\n".
    "   if (opcval & 0x80) {\n".
    "      dest = (dest << 1) | (dest >> 7);\n".
    "      ccr &= 0xf0;".
    "      ccr |= dest & 1;\n".
    "   } else {\n".
    "      int8 c = dest >> 7;".
    "      dest = (dest << 1) | (ccr & 1);\n".
    "      ccr &= 0xf0;".
    "      ccr |= c;\n".
    "   }\n".
    setBRd().
    setNZ("ROTL");
}
sub RotR() {
    getBRd().
    "   if (opcval & 0x70) goto illOpc;\n".
    "   if (opcval & 0x80) {\n".
    "      ccr &= 0xf0;".
    "      ccr |= dest & 1;\n".
    "      dest = (dest >> 1) | (dest << 7);\n".
    "   } else {\n".
    "      int8 c = dest & 1;".
    "      dest = (dest >> 1) | (ccr << 7);\n".
    "      ccr &= 0xf0;".
    "      ccr |= c;\n".
    "   }\n".
    setBRd().
    setNZ("ROTR");
}

######### Logical instructions ########################
sub AndI() {
    "   uint8 nr = (opc >> 8) & 0xf;\n".
	"   uint8 dest = reg[nr];\n".
	"   uint8 src = opcval;\n".
	"   dest &= src;\n".
	setNZV("ANDB"). 
	"   reg[nr] = dest;\n";
}
sub And() {
    return
	getBRd(). 
	getBRs().
	"   dest &= src;\n".
	setNZV("ANDB"). 
        setBRd();
}

sub OrI() {
    "   uint8 nr = (opc >> 8) & 0xf;\n".
    "   uint8 dest = reg[nr];\n".
	"   uint8 src = opcval;\n".
	"   dest |= src;\n".
	setNZV("ANDB"). 
	"   reg[nr] = dest;\n";
}
sub Or() {
    return
	getBRd(). 
	getBRs().
	"   dest |= src;\n".
	setNZV("ANDB"). 
        setBRd();
}

sub XorI() {
    "   uint8 nr = (opc >> 8) & 0xf;\n".
	"   uint8 dest = reg[nr];\n".
	"   uint8 src = opcval;\n".
	"   dest ^= src;\n".
	setNZV("ANDB"). 
	"   reg[nr] = dest;\n";
}
sub Xor() {
    return
	getBRd(). 
	getBRs().
	"   dest ^= src;\n".
	setNZV("ANDB"). 
        setBRd();
}

sub Not() {
    return
	getBRd(). 
	"   if (opcval & 0x70) goto illOpc;\n". 
	"   if (opcval & 0x80) {\n". 
	"      dest = -dest;\n".
	"      ccr &= 0xd0;\n".
	"   ".setNZ("NEGB"). 
	"      if (dest & 0xf) ccr |= 0x20;\n".
	"      if (dest == 0x80) ccr |= 0x2;\n".
	"      if (dest) ccr |= 0x1;\n".
	"   } else { \n".
	"      dest = ~dest;\n".
	"   ".setNZV("NOTB"). 
	"   }\n".
        setBRd();
}


sub build_case($$) {
    my ($hex, $func) = @_;
    return "case 0x$hex:  /* $func */\n".
	qq'   MAKE_LABEL("cpuopc_$hex");\n{\n'.
	&$func( hex $hex ).
	"   break;\n}\n";
}

while (<DATA>) {
    $_ =~ /([0-9a-fA-F][0-9a-fA-FxX])=(\w+)/ or next;
    $hex = $1;
    $func = $2;
    if ($hex =~ /x/) {
	for ($i = 0; $i < 16; $i++) {
	    $hex = substr($hex,0,1) . sprintf("%X", $i);
	    if ($i < 15) {
		print "case 0x$hex:\n";
	    } else {
		print build_case($hex, $func);
	    }
	}
    } else {
	print build_case($hex, $func);
    }
}

__DATA__
00=Nop
01=Sleep
02=StC
03=LdC
04=OrC
05=XorC
06=AndC
07=LdCI
08=AddB
09=AddW
0A=Inc
0B=AddS
0C=MovB
0D=MovW
0E=AddX
0F=DAA
10=ShL
11=ShR
12=RotL
13=RotR
14=Or
15=Xor
16=And
17=Not
18=SubB
19=SubW
1A=Dec
1B=SubS
1C=CmpB
1D=CmpW
1E=SubX
1F=DAS
2x=MovBFA8
3x=MovBTA8
40=BRA
41=BRN
42=BHI
43=BLO
44=BCC
45=BCS
46=BNE
47=BEQ
48=BVC
49=BVS
4A=BPL
4B=BMI
4C=BGE
4D=BLT
4E=BGT
4F=BLE
50=MulXU
51=DivXU
54=RtS
55=BSr
56=RtE
57=Trap
59=JmpRI
5A=JmpA16
5B=JmpAA8
5D=JsrRI
5E=JsrA16
5F=JsrAA8
60=BSet
61=BNot
62=BClr
63=BTst
67=BSt
68=MovBRI
69=MovWRI
6A=MovBA16
6B=MovWA16
6C=MovBRIP
6D=MovWRIP
6E=MovBRID
6F=MovWRID
70=BSetI
71=BNotI
72=BClrI
73=BTstI
74=BOr
75=BXor
76=BAnd
77=BLd
79=MovWI
7B=EepMov
7C=BitInd
7D=BitInd
7E=BitAbs
7F=BitAbs
8x=AddBI
9x=AddXI
Ax=CmpBI
Bx=SubXI
Cx=OrI
Dx=XorI
Ex=AndI
Fx=MovBI
__END__
