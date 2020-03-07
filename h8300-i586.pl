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
# $Id: h8300-i586.pl 314 2006-03-16 18:25:26Z hoenicke $
#

#################
#
# This code builds the x86 assembler part of the hitachi h8300 emulator.
# Only the most common opcodes are implemented. All other opcodes just 
# jump to the cleanup label which will give control to the C part of the
# emulator.
#
#################
#
# The following x86 registers have a special meaning:
#
# These registers always have this meaning:
#  %esi =~ PC
#  %ebp =~ cycles - next_timer_cycles
#  %ebx =~ flags
#
# These registers give the current opcode but may be overwritten:
#  %ecx =~ first byte of opcode
#  %edx =~ opcval (second byte of opcode)
#
# These convention hold for many opcodes:
#  %edi =~ dest register
#  %edx =~ src register  (after opcval was fully used)
#  %eax =~ src value
#  %ecx =~ dest value    (when opcode is no longer important)
#
#################

$WRITE_PATTERN = "a4";
$FAST_PATTERN  = "40";
$READ_PATTERN  = "88";
$CODE_PATTERN  = "80";   # don't check for breakpoint on second word
$READWRITE_PATTERN ="ac";


sub getWRdAddr() {
    "\tmovl\t%edx,%edi\n".
    "\tandl\t\$7,%edi\n"
}
sub getWRdVal() {
    "\tmovb\tEXTERN(reg)(%edi),%ch\n".
    "\tmovb\tEXTERN(reg)+8(%edi),%cl\n"
}
sub getWRd() {
    getWRdAddr().getWRdVal();
}
sub getWRs() {
    "\tshrl\t\$4,%edx\n".
    "\tandl\t\$7,%edx\n".
    "\tmovb\tEXTERN(reg)(%edx),%ah\n".
    "\tmovb\tEXTERN(reg)+8(%edx),%al\n"
}
sub getWRsNoMask() {
    "\tshrl\t\$4,%edx\n".
    "\tmovb\tEXTERN(reg)(%edx),%ah\n".
    "\tmovb\tEXTERN(reg)+8(%edx),%al\n"
}
sub setWRs() {
    "\tmovb\t%ah,EXTERN(reg)(%edx)\n".
    "\tmovb\t%al,EXTERN(reg)+8(%edx)\n"
}
sub getUWRs() {
    "\tmovl\t%edx,%ecx\n".
    "\tshrl\t\$4,%edx\n".
    "\tandl\t\$7,%edx\n".
    "\txorl\t%eax,%eax\n".
    "\tmovb\tEXTERN(reg)(%edx),%ah\n".
    "\tmovb\tEXTERN(reg)+8(%edx),%al\n"
}
sub addUWRs() {
    "\tmovl\t%edx,%ecx\n".
    "\tshrl\t\$4,%edx\n".
    "\tandl\t\$7,%edx\n".
    "\taddb\tEXTERN(reg)+8(%edx),%al\n".
    "\tadcb\tEXTERN(reg)(%edx),%ah\n"
}
sub getWRm() {
    "\tshrl\t\$4,%edx\n".
    "\tandl\t\$7,%edx\n".
    "\tmovb\tEXTERN(reg)(%edx),%ch\n".
    "\tmovb\tEXTERN(reg)+8(%edx),%cl\n";
}
sub getBRdAddr() {
    "\tmovl\t%edx,%edi\n".
    "\tandl\t\$15,%edi\n"
}
sub getBRdVal() {
    "\tmovb\tEXTERN(reg)(%edi),%cl\n"
}
sub getBRd() {
    getBRdAddr().getBRdVal()
}
sub getBRs() {
    "\tshrl\t\$4,%edx\n".
    "\tmovb\tEXTERN(reg)(%edx),%dl\n"
}
sub getBRm() {
    "\tshrl\t\$4,%edx\n".
    "\tmovb\tEXTERN(reg)(%edx),%cl\n"
}

sub getWOpc() {
    return
	"\ttestb\t\$0x$CODE_PATTERN, EXTERN(memtype)+2(%esi)\n".
	"\tjnz\tLOCAL(clean_up)\n".
	"\txorl\t%eax,%eax\n".
	"\tmovb\tEXTERN(memory)+2(%esi),%ah\n".
	"\tmovb\tEXTERN(memory)+3(%esi),%al\n"
}

sub getWOpc2CX() {
    return
	"\ttestb\t\$0x$CODE_PATTERN, EXTERN(memtype)+2(%esi)\n".
	"\tjnz\tLOCAL(clean_up)\n".
	"\txorl\t%ecx,%ecx\n".
	"\tmovb\tEXTERN(memory)+2(%esi),%ch\n".
	"\tmovb\tEXTERN(memory)+3(%esi),%cl\n"
}

sub setNZ($) {
    return
	"\tlahf\n".
	"\tshrb\t\$4,%ah\n".
	"\tandb\t\$12,%ah\n".
	"\torb\t%ah,%bl\n";
}



sub setWRd() {
    return
	"\tmovb\t%ch,EXTERN(reg)(%edi)\n".
        "\tmovb\t%cl,EXTERN(reg)+8(%edi)\n";
}
sub setBRm() {
    return
	"\tmovb\t%cl,EXTERN(reg)(%edi)\n";
}

sub pushW($) {
    return
	"  {uint16 sp = GET_REG16(7) - 2;\n".
	"   WRITE_WORD(sp, $_[0]);\n".
        "   SET_REG16(7, sp);}\n";
}
sub popW($) {
    return
	"  {uint16 sp = GET_REG16(7);\n".
        "   SET_REG16(7, sp + 2);\n".
	"   $_[0] = READ_WORD(sp);}\n";
}


sub illOpc($) {
    my $mask = $_[0];

    "\ttestb\t\$$mask,%dl\n".
    "\tjnz\tLOCAL(illegalOpcode)\n"
}

# add some cycles if slow mem
sub addSlowCycles {
    my $reg = $_[1] || "%eax";
    my $tmpreg = $_[2] || "c";
    return
	"\txorl\t%e${tmpreg}x,%e${tmpreg}x\n".
	"\ttestb\t\$0x$FAST_PATTERN, EXTERN(memtype)($reg)\n".
	"\tsetz\t%${tmpreg}l\n".
	($_[0] == 1
	 ? "\taddl\t%e${tmpreg}x,%ebp\n"
	 : "\tleal\t(%ebp,%e${tmpreg}x,$_[0]),%ebp\n");
}

sub getNextCycle($) {
    my $reg = $_[0];
    return
	"\tmovl\tEXTERN(next_timer_cycle), $reg\n".
	"\torb\t%bl, %bl\n".
	"\tcmovsl\tEXTERN(next_nmi_cycle), $reg\n";
}
sub callMethod($$$) {
    my $method = $_[0];
    my $cleanup = $_[1];
    my $depth = 12 + $_[2];
    return
	"\tmovw\t%si,EXTERN(pc)\n".
	getNextCycle("%eax").
	"\taddl\t%eax,%ebp\n".
	"\tmovl\t%ebp,EXTERN(cycles)\n".
	"\tleal\t$depth(%esp), %ebp\n".
	"\tcall\t$method\n".
	$cleanup.
	"\tmovl\tEXTERN(cycles),%ebp\n".
	getNextCycle("%ecx").
	"\tsubl\t%ecx,%ebp\n";
}


#### low level read / write

sub writeB() {
    return
	"\ttestb\t\$0x$WRITE_PATTERN, EXTERN(memtype)(%eax)\n".
	"\tjnz\tLOCAL(clean_up)\n".
	"\tmovb\t%cl, EXTERN(memory)(%eax)\n";
}
sub writeW() {
    return
	"\ttestw\t\$0x$WRITE_PATTERN$WRITE_PATTERN, EXTERN(memtype)(%eax)\n".
	"\tjnz\tLOCAL(clean_up)\n".
	"\tmovb\t%ch, EXTERN(memory)(%eax)\n".
	"\tmovb\t%cl, EXTERN(memory)+1(%eax)\n";
}
sub readB() {
    return
	"\ttestb\t\$0x$READ_PATTERN, EXTERN(memtype)(%eax)\n".
	"\tjnz\tLOCAL(clean_up)\n".
	"\tmovb\tEXTERN(memory)(%eax), %cl\n";
}

sub writeBport() {
    return
	"\tmovb\t%dl,EXTERN(memory)(%eax)\n".
	"\ttestb\t\$0x80, EXTERN(memtype)(%eax)\n".
	"\tjz\t5f\n".
	"\tmovl\tEXTERN(port)-0x7fc40+4(,%eax,8),%ecx\n".
	"\torl\t%ecx,%ecx\n".
	"\tjz\t5f\n".
	"\tpushl\t%eax\n".
	"\tmovzbl\t%dl,%eax\n".
	"\tpushl\t%eax\n".
	callMethod("*%ecx","",8).
	"\tpopl\t%eax\n".
	"\tpopl\t%eax\n".

	"5:\n";
}

sub readBport($) {
    $pattern = "0x$_[0]";
    $pattern2 = ($pattern & ~0x80);
    return
	"\ttestb\t\$$pattern, EXTERN(memtype)(%eax)\n".
	"\tjz\t5f\n".

	"\ttestb\t\$$pattern2, EXTERN(memtype)(%eax)\n".
	"\tjnz\tLOCAL(clean_up)\n".              # should we handle motor bits???
	"\tcmpl\t\$0xff88, %eax\n".
	"\tjb\tLOCAL(illegaladdr)\n".
	"\tmovl\tEXTERN(port)-0x7fc40(,%eax,8),%edx\n".
	"\torl\t%edx,%edx\n".
	"\tjz\t5f\n".
	"\tpushl\t%eax\n".
	"\tpushl\t%ecx\n".
	callMethod("*%edx","",8).
	"\tmovb\t%al,%dl\n".
	"\tpopl\t%ecx\n".
	"\tpopl\t%eax\n".
	"\tmovb\t%dl, EXTERN(memory)(%eax)\n".

	"5:\n".
	"\tmovb\tEXTERN(memory)(%eax), %dl\n";   # ax = addr, dl = value
}

sub readW() {
    return
	"\ttestw\t\$0x$READ_PATTERN$READ_PATTERN, EXTERN(memtype)(%eax)\n".
	"\tjnz\tLOCAL(clean_up)\n".
	"\tmovb\tEXTERN(memory)(%eax), %ch\n".
	"\tmovb\tEXTERN(memory)+1(%eax), %cl\n";
}

########  Misc instructions ###########################

sub Nop() {
    return
	illOpc(0xff).
	"\tmovb\tEXTERN(memory)+2(%esi),%ch\n".
	"\tmovb\tEXTERN(memory)+3(%esi),%cl\n".
	"\tcmpw\t\$0x5470,\%cx\n".
	"\tje\tLOCAL(clean_up)\n";
}

sub Sleep() {
    $goto="clean_up";
}

sub Trap() {
    $goto="trap";
}

######### Arithmetic instructions ########################

sub AddSubCommon($) {
    my ($ext, $x, $w, $h, $l, $mask, $add, $adc, $opc, $setRd);
    $opc = $_[0];

    if ($opc !~ /W/) {
	if ($common{$opc}) {
	    $noepilogue = 1;
	    return "\tjmp\tLOCAL($common{$opc})\n";
	}
	$common{$opc}="common_$opc";
    }
    
    $ext = $opc =~ /X/ ? 1 : 0;
    $add = ($opc =~ /ADD/ ? "add" : "sub");
    $adc = ($opc =~ /ADD/ ? "adc" : "sbb");
    $add = $adc if $ext;

    if ($opc =~ /W/) {
	$addcmd = "${add}b\t%al,%cl\n\t${adc}b\t%ah,%ch";
	$setRd =  
	    "\torw\t%cx,%cx\n".
	    "\tsetz\t%al\n".
	    "\tshlb\t\$2,%al\n".
	    "\torb\t%al,%bl\n".
	    ($opc =~ /CMP/ ? "" : setWRd());
    } else {
	$add = "cmp" if ($opc =~ /CMP/);
	$addcmd = "${add}b\t%dl,EXTERN(reg)(%ecx)"; # do operation in place
	$setRd = "";
    }    


    return
	($opc =~ /W/ ? "" 
	 : "LOCAL($common{$opc}):\n".
	   "\tandl\t\$0xf,%ecx\n").

	(!$ext ? "\t${addcmd}\n"               # do addition/subtraction
	       : "\tbtsw\t\$0,%bx\n".          # test and set carry
	         "\t${addcmd}\n").             # do addition/subtraction

	"\tseto\t%al\n".
	"\tlahf\n".                            # handle flags
	"\trolw\t\$8,%ax\n".
	($opc =~ /W/ ? "\tandl\t\$0x1bf,%eax\n" : "\tmovzwl\t%ax,%eax\n").
	($opc !~ /SUBX/ ? 
	         "\tandb\t\$0xd0,%bl\n".       # clear flags
	         "\torb\tiflags2ccr(%eax),%bl\n"
	       : "\torb\t\$0x2b,%bl\n".        # set flags except zero
	         "\tmovb\tiflags2ccr(%eax),%al\n".
	         "\torb\t\$0xd0,%al\n".
	         "\tandb\t%al,%bl\n").         # be careful not to set zero
	$setRd;
}

sub AddBI() {
    return
	AddSubCommon("ADDB");
}
sub AddB() {
    return
	"\tmovl\t%edx,%ecx\n".
	getBRs().
	AddSubCommon("ADDB")
}
sub AddW() {
    return
	illOpc(0x88).
	getWRd(). 
	getWRsNoMask().
	AddSubCommon("ADDW")
}
sub AddS() {    
    return
	illOpc(0x78).
	getWRdAddr().
	"\taddb\t%dl,%dl\n".
	"\tadcb\t\$1,EXTERN(reg)+8(%edi)\n".
	"\tadcb\t\$0,EXTERN(reg)(%edi)\n"
}
sub AddXI() {
    return
	AddSubCommon("ADDX")
}

sub AddX() {
    return
	"\tmovl\t%edx,%ecx\n".
	getBRs().
	AddSubCommon("ADDX")
}

sub CmpBI() {
    return
	AddSubCommon("CMPB");
}
sub CmpB() {
    return
	"\tmovl\t%edx,%ecx\n".
	getBRs().
	AddSubCommon("CMPB");
}
sub CmpW() {
    return
	illOpc(0x88).
	getWRd(). 
	getWRsNoMask().
	AddSubCommon("CMPW");
}

sub SubB() {
    return
	"\tmovl\t%edx,%ecx\n".
	getBRs().
	AddSubCommon("SUBB")
}
sub SubW() {
    return
	illOpc(0x88).
	getWRd(). 
	getWRsNoMask().
	AddSubCommon("SUBW")
}
sub SubS() {    
    return
	illOpc(0x78).
	getWRdAddr().
	"\taddb\t%dl,%dl\n".
	"\tsbbb\t\$1,EXTERN(reg)+8(%edi)\n".
	"\tsbbb\t\$0,EXTERN(reg)(%edi)\n"
}
sub SubXI() {
    return
	AddSubCommon("SUBX")
}

sub SubX() {
    return
	"\tmovl\t%edx,%ecx\n".
	getBRs().
	AddSubCommon("SUBX")
}

sub Inc() {
    return
	illOpc(0xf0).
	getBRdAddr().

	"\tandb\t\$0xf1,%bl\n".

	"\tincb\tEXTERN(reg)(%edi)\n".

	"\tlahf\n".
	"\tseto\t%al\n".
	"\taddb\t%al,%al\n".
	"\tshrb\t\$4,%ah\n".
	"\tandb\t\$12,%ah\n".
	"\torb\t%al,%bl\n".
	"\torb\t%ah,%bl\n";
}
sub Dec() {
    return
	illOpc(0xf0).
	getBRdAddr().

	"\tandb\t\$0xf1,%bl\n".

	"\tdecb\tEXTERN(reg)(%edi)\n".

	"\tlahf\n".
	"\tseto\t%al\n".
	"\taddb\t%al,%al\n".
	"\tshrb\t\$4,%ah\n".
	"\tandb\t\$12,%ah\n".
	"\torb\t%al,%bl\n".
	"\torb\t%ah,%bl\n";
}


sub DAA() {
    $goto="clean_up";
}
sub DAS() {
    $goto="clean_up";
}
sub MulXU() {
    $goto="clean_up";
}
sub DivXU() {
    $goto="clean_up";
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

sub BitAbs1() {
    $noepilogue=1;
    return
	"\txorl\t%eax,%eax\n".
	"\tmovsbw\t%dl,%ax\n".
	"\tjmp\tLOCAL(bitops1)\n";
}

sub BitInd1() {
    $extraopclen++;
    $extracycles+=4;
    return
	illOpc(0x8f).
	"\tshrl\t\$4,%edx\n".
	"\txorl\t%eax,%eax\n".
	"\tmovb\tEXTERN(reg)+8(%edx),%al\n".
	"\tmovb\tEXTERN(reg)(%edx),%ah\n".

	"LOCAL(bitops1):\n".

	"\ttestb\t\$0x$CODE_PATTERN,EXTERN(memtype)+2(%esi)\n".
	"\tjnz\tLOCAL(clean_up)\n".
	"\txorl\t%ecx,%ecx\n".
	"\tmovb\tEXTERN(memory)+3(%esi),%cl\n".
	"\ttestb\t\$0x0f,%cl\n".
	"\tjnz\tLOCAL(illegalOpcode)\n".         # cl = shift param
	"\tshrb\t\$4,%cl\n".

	"\txorl\t%edx,%edx\n".
	"\tmovb\tEXTERN(memory)+2(%esi),%dl\n".
	"\txorl\t\$0x60,%edx\n".
	"\ttestl\t\$~0x13,%edx\n".
	"\tjz\t1f\n".
	"\tcmpl\t\$7,%edx\n".               # Check for BSt
	"\tjnz\tLOCAL(clean_up)\n".
	"\tmovb\t%cl,%dl\n".                # convert BSt to BSet/BClr
	"\tshrb\t\$3,%dl\n".
	"\txorb\t%bl,%dl\n".
	"\txorb\t\$1,%dl\n".
	"\taddb\t%dl,%dl\n".
	"\tandb\t\$7,%cl\n".
	"\tjmp\t2f\n".
	"1:\ttestl\t\$0x10,%edx\n".    # get shift count (register or immediate)
	"\tjnz\t1f\n".
	"\tmovb\tEXTERN(reg)(%ecx),%cl\n".
	"\tandb\t\$7,%cl\n".
	"1:\n".
	"\ttestb\t\$8,%cl\n".
	"\tjnz\tLOCAL(illegalOpcode)\n".
	"2:\tandl\t\$3,%edx\n".
	"\tcmpl\t\$3,%edx\n".
	"\tjz\tLOCAL(illegalOpcode)\n".
	"\tmovl\t%edx,%edi\n".            # di = opcode & 0x3

	readBport($READWRITE_PATTERN).

	"\tmovb\t\$1,%dh\n".         # dh = bit mask
	"\tshlb\t%cl,%dh\n".
	"\tcmpl\t\$1,%edi\n".
	"\tje\t1f\n".                # BNot -> 1
	"\torb\t%dh,%dl\n".          # BSet or BClr
	"\torl\t%edi,%edi\n".
	"\tje\t2f\n".
	"1:\txorb\t%dh,%dl\n".       # BNot or BClr
	"2:\n".

	writeBport().

	addSlowCycles(2);            # add 2 cycle if slow mem
}

sub BitAbs0() {
    $goto="clean_up";
}
	
sub BitInd0() {
    $goto="clean_up";
}	

sub BAnd() {
    $goto="clean_up";
}
sub BLd() {
    $goto="clean_up";
}
sub BSt() {
    $goto="clean_up";
}


sub BOr() {
    $goto="clean_up";
}
sub BXor() {
    $goto="clean_up";
}


sub BClrI() {
    return
	illOpc(0x80).
	"\tmovb\t%dl,%cl\n".
	"\tshrb\t\$4,%cl\n".
	"\tmovb\t\$1,%al\n".
	"\tshlb\t%cl,%al\n".
	"\tandl\t\$15,%edx\n".
	"\tnotb\t%al\n".
	"\tandb\t%al,EXTERN(reg)(%edx)\n";
}

sub BClr() {
    $goto="clean_up";
}

sub BSetI() {
    return
	illOpc(0x80).
	"\tmovb\t%dl,%cl\n".
	"\tshrb\t\$4,%cl\n".
	"\tmovb\t\$1,%al\n".
	"\tshlb\t%cl,%al\n".
	"\tandl\t\$15,%edx\n".
	"\torb\t%al,EXTERN(reg)(%edx)\n";
}

sub BSet() {
    $goto="clean_up";
}

sub BNotI() {
    return
	illOpc(0x80).
	"\tmovb\t%dl,%cl\n".
	"\tshrb\t\$4,%cl\n".
	"\tmovb\t\$1,%al\n".
	"\tshlb\t%cl,%al\n".
	"\tandl\t\$15,%edx\n".
	"\txorb\t%al,EXTERN(reg)(%edx)\n";
}
sub BNot() {
    $goto="clean_up";
}
sub BTstI() {
    return
	illOpc(0x80).
	"\tmovb\t%dl,%cl\n".
	"\tandl\t\$15,%edx\n".
	"\tshrb\t\$4,%cl\n".
	"\tmovb\tEXTERN(reg)(%edx),%al\n".
	"\tshrb\t%cl,%al\n".
	"\tandb\t\$1,%al\n".
	"\torb\t\$4,%bl\n".
	"\tshlb\t\$2,%al\n".
	"\txorb\t%al,%bl\n";
}
sub BTst() {
    return
	getBRdAddr().
    	getBRm().
	"\tmovb\tEXTERN(reg)(%edi),%al\n".
	"\tandb\t\$7,%cl\n".
	"\tshrb\t%cl,%al\n".
	"\tandb\t\$1,%al\n".
	"\torb\t\$4,%bl\n".
	"\tshlb\t\$2,%al\n".
	"\txorb\t%al,%bl\n";
}

######### Branch instructions ########################

sub BRA() {
    $extracycles = 2;
    return 
	"LOCAL(do_bra):\n".
	illOpc(0x01).
	addSlowCycles(4, "%esi").            # add 4 cycles if slow mem
	"\tmovsbl\t%dl, %edx\n".
	"\taddl\t%edx, %esi\n";
}
sub BRN() {
    $extracycles = 2;
    return 
	addSlowCycles(4, "%esi");            # add 4 cycles if slow mem
}
sub BHI() {
    $extracycles = 2;
    return "\ttestb\t\$5,%bl\n".
	"\tjz\tLOCAL(do_bra)\n".
	addSlowCycles(4, "%esi");            # add 4 cycles if slow mem

}
sub BLO() {
    $extracycles = 2;
    return "\ttestb\t\$5,%bl\n".
	"\tjnz\tLOCAL(do_bra)\n".
	addSlowCycles(4, "%esi");            # add 4 cycles if slow mem
	
}
sub BCC() {
    $extracycles = 2;
    return "\ttestb\t\$1,%bl\n".
	"\tjz\tLOCAL(do_bra)\n".
	addSlowCycles(4, "%esi");            # add 4 cycles if slow mem

}
sub BCS() {
    $extracycles = 2;
    return "\ttestb\t\$1,%bl\n".
	"\tjnz\tLOCAL(do_bra)\n".
	addSlowCycles(4, "%esi");            # add 4 cycles if slow mem
}
sub BNE() {
    $extracycles = 2;
    return "\ttestb\t\$4,%bl\n".
	"\tjz\tLOCAL(do_bra)\n".
	addSlowCycles(4, "%esi");            # add 4 cycles if slow mem

}
sub BEQ() {
    $extracycles = 2;
    return "\ttestb\t\$4,%bl\n".
	"\tjnz\tLOCAL(do_bra)\n".
	addSlowCycles(4, "%esi");            # add 4 cycles if slow mem

}
sub BVC() {
    $extracycles = 2;
    return "\ttestb\t\$2,%bl\n".
	"\tjz\tLOCAL(do_bra)\n".
	addSlowCycles(4, "%esi");            # add 4 cycles if slow mem

}
sub BVS() {
    $extracycles = 2;
    return "\ttestb\t\$2,%bl\n".
	"\tjnz\tLOCAL(do_bra)\n".
	addSlowCycles(4, "%esi");            # add 4 cycles if slow mem

}
sub BPL() {
    $extracycles = 2;
    return "\ttestb\t\$8,%bl\n".
	"\tjz\tLOCAL(do_bra)\n".
	addSlowCycles(4, "%esi");            # add 4 cycles if slow mem

}
sub BMI() {
    $extracycles = 2;
    return "\ttestb\t\$8,%bl\n".
	"\tjnz\tLOCAL(do_bra)\n".
	addSlowCycles(4, "%esi");            # add 4 cycles if slow mem
}
sub BGE() {
    $extracycles = 2;
    return "\tmovb\t%bl,%al\n".
	"\tshrb\t\$2,%al\n".
	"\txorb\t%bl,%al\n".
	"\tandb\t\$2,%al\n".
	"\tjz\tLOCAL(do_bra)\n".
	addSlowCycles(4, "%esi");            # add 4 cycles if slow mem

}
sub BLT() {
    $extracycles = 2;
    return "\tmovb\t%bl,%al\n".
	"\tshrb\t\$2,%al\n".
	"\txorb\t%bl,%al\n".
	"\tandb\t\$2,%al\n".
	"\tjnz\tLOCAL(do_bra)\n".
	addSlowCycles(4, "%esi");            # add 4 cycles if slow mem

}
sub BGT() {
    $extracycles = 2;
    return "\tmovb\t%bl,%al\n".
	"\tshrb\t\$2,%al\n".
	"\tandb\t\$2,%al\n".
	"\txorb\t%bl,%al\n".
	"\tandb\t\$6,%al\n".
	"\tjz\tLOCAL(do_bra)\n".
	addSlowCycles(4, "%esi");            # add 4 cycles if slow mem
}
sub BLE() {
    $extracycles = 2;
    return "\tmovb\t%bl,%al\n".
	"\tshrb\t\$2,%al\n".
	"\tandb\t\$2,%al\n".
	"\txorb\t%bl,%al\n".
	"\tandb\t\$6,%al\n".
	"\tjnz\tLOCAL(do_bra)\n".
	addSlowCycles(4, "%esi");            # add 4 cycles if slow mem

}

sub JmpRI() {
    $noepilogue=1;
    return
	illOpc(0x8f).
	"\tshrl\t\$4,%edx\n".
	"\tmovb\tEXTERN(reg)(%edx),%ch\n".
	"\tmovb\tEXTERN(reg)+8(%edx),%cl\n".
	"\tjmp\tLOCAL(jmpcommon)\n";
}

sub JmpCommon() {
    $extracycles = 4;   # read pc (either lookahead or normal)
    $extraopclen--;     # prevent increment of pc
    return
	"LOCAL(jmpcommon):\n".
	"\ttestl\t\$1,%ecx\n".
	"\tjnz\tLOCAL(unaligned)\n".
	"\ttestb\t\$0x80,EXTERN(memtype)(%ecx)\n".
	"\tjnz\tLOCAL(unaligned)\n".
	addSlowCycles(8, "%esi", "a").            # add 2x4 cycles if slow mem
	"\tmovl\t%ecx,%esi\n";
}

sub JmpA16() {
    return 
	illOpc(0xff).
	getWOpc2CX.
	"\taddl\t\$2,%ebp\n".                  # internal cycles
	JmpCommon();
}
sub JmpAA8() {
    $goto="clean_up";
}

sub JsrCommon() {
    $extracycles = 0;      # cycles handled below
    $extraopclen--;        # prevent increment of pc
    $code =
	"\ttestl\t\$1, %eax\n".
	"\tjnz\tLOCAL(clean_up)\n".
	"\tmovb\tEXTERN(reg)+7,%ch\n".
	"\tmovb\tEXTERN(reg)+8+7,%cl\n".
	"\tsubl\t\$2, %ecx\n".
	"\ttestw\t\$0x$WRITE_PATTERN$WRITE_PATTERN, EXTERN(memtype)(%ecx)\n".
	"\tjnz\tLOCAL(clean_up)\n".
	"\tmovl\t%eax,%esi\n".
	"\tmovb\t%dh, EXTERN(memory)(%ecx)\n".
	"\tmovb\t%dl, EXTERN(memory)+1(%ecx)\n".
	"\tmovb\t%ch, EXTERN(reg)+7\n".
	"\tmovb\t%cl, EXTERN(reg)+8+7\n".
	"\taddl\t\$6,%ebp\n".
	addSlowCycles(8, "%edx", "a").  # add 8 cycles if slow pc
	addSlowCycles(4, "%ecx", "a").  # add 4 cycles if slow stack
	"\tpushl\t\$0\n".
	"\tpushl\t%ecx\n".
	callMethod("EXTERN(frame_begin)", "\taddl\t\$8, %esp\n", 8);

    $extracycles = 0;
    return $code;
}

sub BSr() {
    $noepilogue=1;
    return
	"\tmovsbl\t%dl,%eax\n".
	"\tleal\t2(%esi),%edx\n".
	"\taddl\t%edx,%eax\n".
	"\tjmp\tLOCAL(jsrcommon)\n";
}

sub JsrRI() {
    $noepilogue=1;
    return
	illOpc(0x8f).
	"\txorl\t%eax,%eax\n".
	getWRsNoMask().
	"\tleal\t2(%esi),%edx\n".
	"\tjmp\tLOCAL(jsrcommon)\n";
}
sub JsrA16() {
    return
	illOpc(0xff).
	getWOpc.
	"\taddl\t\$2,%ebp\n".                  # internal cycles
	"\tleal\t4(%esi),%edx\n".
	"LOCAL(jsrcommon:)\n".
	JsrCommon();
}

sub JsrAA8() {
    $goto="clean_up";
}


sub Return($) {
    $rte = $_[0];
    $extracycles = 2+6 + ($rte ? 2 : 0); # internal + 2xread pc + 1xread stack
                                         #          + 1xread stack for rte
    $extraopclen--;                      # prevent increment of pc
    return
	"\tmovb\tEXTERN(reg)+7,%ch\n".
	"\tmovb\tEXTERN(reg)+8+7,%cl\n".
	"\tmovl\t%ecx, %edi\n".
	($rte
	 ? "\ttestl\t\$0x$READ_PATTERN$READ_PATTERN$READ_PATTERN$READ_PATTERN, EXTERN(memtype)(%ecx)\n"
	 : "\ttestw\t\$0x$READ_PATTERN$READ_PATTERN, EXTERN(memtype)(%ecx)\n").
	"\tjnz\tLOCAL(clean_up)\n".
	($rte ? "\taddl\t\$2, %ecx\n" : "").
	"\tpushl\t\$$rte\n".
	"\tpushl\t%ecx\n".
	callMethod("EXTERN(frame_end)", "\taddl\t\$8, %esp\n".
		   ($rte ? "\tmovb\tEXTERN(memory)(%edi),%bl\n": ""), 8).
	"\txorl\t%ecx,%ecx\n".
	"\tmovb\tEXTERN(memory)".($rte?"+2":""  )."(%edi),%ch\n".
	"\tmovb\tEXTERN(memory)".($rte?"+3":"+1")."(%edi),%cl\n".
	"\ttestl\t\$1,%ecx\n".
	"\tjnz\tLOCAL(unaligned)\n".
	"\ttestb\t\$0x80,EXTERN(memtype)(%ecx)\n".
	"\tjnz\tLOCAL(unaligned)\n".
	"\tmovl\t%edi,%eax\n".
	"\taddl\t\$".($rte?"4":"2").",%eax\n".
	"\tmovb\t%ah,EXTERN(reg)+7\n".
	"\tmovb\t%al,EXTERN(reg)+8+7\n".
	addSlowCycles(8, "%esi", "a").            # add 8 cycles if slow pc
	addSlowCycles($rte ? 8 : 4, "%edi", "a"). # add 4/8 cycles if slow stack
	"\tmovl\t%ecx,%esi\n";
}

sub RtS() {
    Return(0);
}
sub RtE() {
    Return(1);
}

######### Move instructions ########################

sub MovBCore {
    $extracycles += 2;
    $extracycles += 2 if ($_[0] =~ /P/);

    $dbit = $_[0] =~ /A16/ ? "%dl" : "%cl";

    return
	"\ttestb\t\$0x80,$dbit\n".
	"\tjz\t1f\n".
	($_[0] =~ /P/ ? "\tsubw\t\$1,%ax\n" : "").
	"\tmovb\tEXTERN(reg)(%edi),%cl\n".
	writeB().
	($_[0] =~ /P/ ? setWRs() : "").
	"\tjmp\t2f\n".
	"1:\n".
	readB().
	setBRm().
	($_[0] =~ /P/ ? "\taddl\t\$1,%eax\n".setWRs() : "").
	"2:\n".
	addSlowCycles(1, "%eax", "d").            # add 1 cycle if slow mem
	"\tandb\t\$0xf1,%bl\n".
	"\torb\t%cl,%cl\n".
	setNZ("MOVB");
}

sub MovWCore {
    $extracycles += 2;
    $extracycles += 2 if ($_[0] =~ /P/);
    $dbit = $_[0] =~ /A16/ ? "%dl" : "%cl";

    return
	"\ttestb\t\$0x80,$dbit\n".
	"\tjz\t1f\n".
	"\tmovb\tEXTERN(reg)(%edi),%ch\n".
	"\tmovb\tEXTERN(reg)+8(%edi),%cl\n".
	($_[0] =~ /P/ ? "\tsubw\t\$2,%ax\n" : "").
	writeW().
	($_[0] =~ /P/ ? setWRs() : "").
	"\tjmp\t2f\n".
	"1:\n".
	readW().
	setWRd().
	($_[0] =~ /P/ ? "\taddl\t\$2,%eax\n".setWRs() : "").
	"2:\n".
	addSlowCycles(4, "%eax", "d").            # add 4 cycles if slow mem
	"\tandb\t\$0xf1,%bl\n".
	"\torw\t%cx,%cx\n".
	setNZ("MOVW");
}

sub EepMov() {
    $goto="clean_up";
}

sub MovB() {
    return
	getBRdAddr().
	getBRm().
	"\tandb\t\$0xf1,%bl\n".
	"\tandb\t\$0xf1,%bl\n".
	"\torb\t%cl,%cl\n".
	setNZ("MOVB").
	setBRm();
}
sub MovBI() {
    return
	"\tandb\t\$0xf1,%bl\n".
	"\tandl\t\$0x0f,%ecx\n".
	"\tmovb\t%dl,EXTERN(reg)(%ecx)\n".
	"\torb\t%dl,%dl\n".
	setNZ("MOVB");
}

sub MovBRI() {
    return
	getBRdAddr().
	getUWRs().
	MovBCore();
}
sub MovBRID() {
    $extraopclen++;
    return
	getWOpc().
	getBRdAddr().
	addUWRs().
	MovBCore();
}
sub MovBRIP() {
    return
	getBRdAddr().
	getUWRs().
	MovBCore("P")
}
sub MovBFA8() {
    $extracycles += 2;
    return
	"\tmovzbl\t%cl,%edi\n".
	"\tandl\t\$0x0f,%edi\n".
	"\tmovsbw\t%dl,%ax\n".
	"\tmovzwl\t%ax,%eax\n".

	readBport($READ_PATTERN).
	addSlowCycles(1).

	"\tmovb\t%dl,EXTERN(reg)(%edi)\n".
	"\tandb\t\$0xf1,%bl\n".
	"\torb\t%dl,%dl\n".
	setNZ("MOVB");
}
sub MovBTA8() {
    $extracycles += 2;
    return
	"\tmovzbl\t%cl,%edi\n".
	"\tandl\t\$0x0f,%edi\n".
	"\tmovsbw\t%dl,%ax\n".
	"\tmovzwl\t%ax,%ecx\n".
	
	"\ttestb\t\$0x$WRITE_PATTERN, EXTERN(memtype)(%ecx)\n".
	"\tjz\t5f\n".
	"\ttestb\t\$0x24, EXTERN(memtype)(%ecx)\n".
	"\tjnz\tLOCAL(clean_up)\n".              # should we handle motor bits???
	"\tcmpl\t\$0xff88, %ecx\n".
	"\tjb\tLOCAL(illegaladdr)\n".
	"\t5:\n".

	"\tmovb\tEXTERN(reg)(%edi),%dl\n".
	"\tandb\t\$0xf1,%bl\n".
	"\torb\t%dl,%dl\n".
	setNZ("MOVB").
	"\tmovl\t%ecx,%eax\n".

	writeBport().
	addSlowCycles(1);
}
sub MovBA16() {
    $extraopclen++;
    return
	illOpc(0x70).
	getWOpc().
	getBRdAddr().
	MovBCore("A16");
}

sub checkFrameSwitch() {
    return
	"\tcmpl\t\$7,%edi\n".
	"\tjz LOCAL(clean_up)\n";
}

sub MovW() {
    return
	illOpc(0x88).
	getWRdAddr().
	checkFrameSwitch().
	getWRm().
	"\tandb\t\$0xf1,%bl\n".
	"\torw\t%cx,%cx\n".
	setNZ("MOVW").
	setWRd();
}
sub MovWI() {
    $extraopclen++;
    return
	illOpc(0xf8).
	getWRdAddr().
	checkFrameSwitch().
	getWOpc2CX.
	"\tandb\t\$0xf1,%bl\n".
	"\torw\t%cx,%cx\n".
	setNZ("MOVW").
	setWRd();
}

sub MovWRI() {
    return
	illOpc(0x08).
	getWRdAddr().
	checkFrameSwitch().
	getUWRs().
	MovWCore();
}
sub MovWRID() {
    $extraopclen++;
    return
	illOpc(0x08).
	getWOpc().
	getWRdAddr().
	checkFrameSwitch().
	addUWRs().
	MovWCore();
}
sub MovWRIP() {
    return
	illOpc(0x08).
	getWRdAddr().
	checkFrameSwitch().
	getUWRs().
	MovWCore("P")
}
sub MovWA16() {
    $extraopclen++;
    return
	illOpc(0x78).
	getWOpc().
	getWRdAddr().
	checkFrameSwitch().
	MovWCore("A16");
}


###### Flag instructions are not handled because of irq_disabled_one probs
sub StC() {
    return
	illOpc(0xf0).
	"\tmovb\t%bl,EXTERN(reg)(%edx)\n";
}
sub LdC() {
    $goto = "clean_up";
}
sub LdCI() {
    $goto = "clean_up";
}
sub AndC() {
    $goto = "clean_up";
}
sub OrC() {
    $goto = "clean_up";
}
sub XorC() {
    $goto = "clean_up";
}

######### Shift instructions ########################
sub ShL() {
    illOpc(0x70).
	getBRdAddr().
	"\tandb\t\$0xf0,%bl\n".
	"\tshlb\t\$1,EXTERN(reg)(%edi)\n".     # do shift
	"\tseto\t%al\n".
	"\tlahf\n".                     # handle flags
	"\tshrb\t\$7,%dl\n".            # clear overflow if not SALL 
	"\tandb\t%dl,%al\n".
	"\trolw\t\$8,%ax\n".
	"\tandl\t\$0x1ef,%eax\n".          # clear aux flag
	"\torb\tiflags2ccr(%eax),%bl\n";
}

sub ShR() {
    $goto="clean_up";
}
sub RotL() {
    $goto="clean_up";
}
sub RotR() {
    $goto="clean_up";
}

######### Logical instructions ########################
sub LogicI($) {
    $op = $_[0];
    return
	"\tandb\t\$0xf1,%bl\n".
	"\tandl\t\$0x0f,%ecx\n".
	"\t\L$op\E\t%dl,EXTERN(reg)(%ecx)\n".
	setNZ($op);
}
sub Logic($) {
    $op = $_[0];
    return
	"\tandb\t\$0xf1,%bl\n".
	"\tmovl\t%edx,%ecx\n".
	"\tshrl\t\$4,%edx\n".
	"\tandl\t\$0x0f,%ecx\n".
	"\tmovb\tEXTERN(reg)(%edx),%al\n".
	"\t\L$op\E\t%al,EXTERN(reg)(%ecx)\n".
	setNZ($op);
}

sub AndBI() {
    LogicI("ANDB");
}
sub AndB() {
    Logic("ANDB");
}

sub OrBI() {
    LogicI("ORB");
}
sub OrB() {
    Logic("ORB");
}

sub XorBI() {
    LogicI("XORB");
}
sub XorB() {
    Logic("XORB");
}

sub Not() {
    $goto="clean_up";
}


sub build_case($$) {
    my ($hex, $func) = @_;
    $noepilogue  = 0;
    $extraopclen = 0;
    $extracycles = 0;
    $goto = "opc$hex";
    $helper = "";
    $case = &$func();

    $goto = "clean_up" if 0;
    if ($hex =~ /x/) {
	$val = hex (substr($hex,0,1)."0");
	for ($i = 0; $i < 16; $i++) {
	    $opctable[$val+$i] = $goto;
	}
    } else {
	$opctable[hex $hex] = $goto;
    }

    return if ($goto ne "opc$hex");

    $opclen     = 2 + 2*$extraopclen;
    $cycles     = $opclen +  $extracycles;
    $slowcycles = (6 - 2) * ($opclen/2);

    if ($noepilogue) {
	$epilogue = "";
    } elsif(!$extracycles && !$extraopclen) {
	$epilogue = "\tjmp\tLOCAL(default_epilogue)\n";
    } elsif (!$extraopclen) {
	$epilogue = 
	    "\taddl\t\$$cycles,%ebp\n".
	    "\tjmp\tLOCAL(default_epilogue_nocycles)\n";
    } else {
	$epilogue =
	    ($opclen 

	     ? "\taddl\t\$$opclen,%esi\n".
	     ($cycles ? "\taddl\t\$$cycles,%ebp\n" : "").
	     "\txorl\t%eax,%eax\n".
	     "\ttestb\t\$0x$FAST_PATTERN, EXTERN(memtype)-$opclen(%esi)\n".
	     "\tsetz\t%al\n".
	     "\tleal\t(%ebp,%eax,$slowcycles),%ebp\n"

	     : ($cycles ? "\taddl\t\$$cycles,%ebp\n" : "")).

	    "\tjmp\tLOCAL(loop)\n";
    }

    return
	"# ($func)\n".
	"\t.align 16\n".
	"LOCAL(opc$hex):\n".
	$case.
	$epilogue.
	$helper;
}

@opctable = ("illegalOpcode") x 256;

while (<DATA>) {
    $_ =~ /([0-9a-fA-F][0-9a-fA-FxX])=(\w+)/ or next;
    $hex = $1;
    $func = $2;
    print build_case($hex, $func);
}

print "\t.section\t.rodata\nopctable:\n";
print "\t.long\tLOCAL($_)\n" foreach (@opctable);
	   
print "\niflags2ccr:";
for ($i = 0; $i< 512; $i++) {
    if (($i & 7) == 0) {
	print "\n\t.byte\t";
    } else {
	print ", ";
    }
    my $ccr = 0;
    $ccr |=  1 if ($i &   1); # carry
    $ccr |=  2 if ($i & 256); # overflow
    $ccr |=  4 if ($i &  64); # zero
    $ccr |=  8 if ($i & 128); # sign
    $ccr |= 32 if ($i &  16); # aux
    printf "0x%02x", $ccr;
}
print "\n";

__DATA__
Ax=CmpBI
1C=CmpB
1D=CmpW
8x=AddBI
08=AddB
9x=AddXI
0E=AddX
09=AddW
0B=AddS
18=SubB
Bx=SubXI
1E=SubX
19=SubW
1B=SubS
Cx=OrBI
Dx=XorBI
Ex=AndBI
Fx=MovBI
00=Nop
01=Sleep
02=StC
03=LdC
04=OrC
05=XorC
06=AndC
07=LdCI
0A=Inc
0C=MovB
0D=MovW
0F=DAA
10=ShL
11=ShR
12=RotL
13=RotR
14=OrB
15=XorB
16=AndB
17=Not
1A=Dec
1F=DAS
2x=MovBFA8
3x=MovBTA8
41=BRN
42=BHI
43=BLO
44=BCC
45=BCS
46=BNE
47=BEQ
4A=BPL
4B=BMI
40=BRA
4C=BGE
4D=BLT
4E=BGT
4F=BLE
49=BVS
48=BVC
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
7C=BitInd0
7D=BitInd1
7E=BitAbs0
7F=BitAbs1
__END__
