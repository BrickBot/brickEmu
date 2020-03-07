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
# $Id: h8300-sparc.pl 325 2006-03-17 15:42:07Z hoenicke $
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
#  %o1 =~ ecx =~ first byte of opcode * 4
#  %o0 =~ edx =~ opcval (second byte of opcode)
#
# These convention hold for many opcodes:
#  %o5 =~ %edi =~ dest register
#  %o3 =~ %edx =~ src register  (after opcval was fully used)
#  %o0 =~ %eax =~ src value
#  %o1 =~ %ecx =~ dest value    (when opcode is no longer important)
#
#################

$WRITE_PATTERN = "a4";
$FAST_PATTERN  = "40";
$READ_PATTERN  = "88";
$CODE_PATTERN  = "80";   # don't check for breakpoint on second word
$READWRITE_PATTERN ="ac";

$destreg = "\%o5";
$destval = "\%o1";
$srcreg  = "\%o3";
$value  = $opcval  = "\%o0";
$opcode  = "\%o1";
$bitnr   = "\%o1";


sub getWRdAddr() {
    "\tandl\t$opcval,7,$destreg\n".
    "\taddl\t$destreg,REG,$destreg\n"
}
sub getWRdVal() {
    "\tldub\t[$destreg],%o7\n".
    "\tldub\t[$destreg+8],$destval\n".
    "\tsll\t%o7,8,%o7\n".
    "\tor\t%o7,$destval,$destval\n"
}
sub getWRd() {
    getWRdAddr().getWRdVal();
}
sub getWRs() {
    "\tsrl\t$opcval,4,$value\n".
    "\tand\t$value,7,$value\n".
    "\taddl\t$value,REG,$value\n".
    "\tldub\t[$value],%o7\n".
    "\tldub\t[$value+8],$value\n".
    "\tsll\t%o7,8,%o7\n".
    "\tor\t%o7,$value,$value\n"
}
sub getWRsNoMask() {
    "\tsrl\t$opcval,4,$value\n".
    "\taddl\t$value,REG,$value\n".
    "\tldub\t[$value],%o7\n".
    "\tldub\t[$value+8],$value\n".
    "\tsll\t%o7,8,%o7\n".
    "\tor\t%o7,$value,$value\n"
}
sub setWRs() {
    "\tstb\t$value,[$destreg+8]\n".
    "\tsrl\t$value,8,$value\n".
    "\tstb\t$value,[$destreg]\n"
}
sub getUWRs() {
    "\tsrl\t$opcval,4,$value\n".
    "\tand\t$value,7,$value\n".
    "\taddl\t$value,REG,$value\n".
    "\tldub\t[$destreg],%o7\n".
    "\tldub\t[$destreg+8],$value\n".
    "\tsll\t%o7,8,%o7\n".
    "\tor\t%o7,$value,$value\n"
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
    "\tand\t$opcval,15,$destreg\n"
}
sub getBRdVal() {
    "\tldsb\t[REG+$destreg],$destval\n"
}
sub getBRd() {
    getBRdAddr().getBRdVal()
}
sub getBRs() {
    "\tsrl\t$opcval,4,$value\n".
    "\tldsb\t[REG+$value],$value"
}
sub getBRm() {
    "\tshrl\t\$4,%edx\n".
    "\tmovb\tEXTERN(reg)(%edx),%cl\n"
}

sub getWOpc() {
    return
	"\ttestb\t\$0x$CODE_PATTERN, EXTERN(memtype)+2(%esi)\n".
	"\tjnz\tclean_up\n".
	"\txorl\t%eax,%eax\n".
	"\tmovb\tEXTERN(memory)+2(%esi),%ah\n".
	"\tmovb\tEXTERN(memory)+3(%esi),%al\n"
}

sub getWOpc2CX() {
    return
	"\ttestb\t\$0x$CODE_PATTERN, EXTERN(memtype)+2(%esi)\n".
	"\tjnz\tclean_up\n".
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

sub setNZV($) {
    my $w = $_[0];
    my $mask = (1<<$w) -1;
    return
	"\tsra\t$value,".($w-1).",%o7\n".
	"\tldub\t[HI2CCR+%o7],%o7\n".
	"\tand\tCCR,0xd1,CCR\n".
	"\tbtst\t$mask,$value\n".
	"\tbne\t1f\n".
	"\tor\tCCR,%o7,CCR\n".
	"\tor\tCCR,4,CCR\n".     # zero
	"1:\n";
}
sub setNZVAC($) {
    my $w = $_[0];
    my $mask = (1<<$w) -1;
    return
	"\tsra\t$value,".($w-1).",%o7\n".
	"\tldub\t[HI2CCR+%o7],%o7\n".
	"\tand\tCCR,0xd0,CCR\n".
	"\tbcc\t1f\n".
	"\tbtst\t$mask,$value\n".
	"\tor\tCCR,1,CCR\n".     # carry
	"1:\tbne\t1f\n".
	"\tor\tCCR,%o7,CCR\n".
	"\tor\tCCR,4,CCR\n".     # zero
	"1:\n";
    #FIXME: aux carry
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

sub illOpc($) {
    my $mask = $_[0];

    "\tbtst\t$mask,$opcval\n".
    "\tbne\tillegalOpcode\n"
}

# add some cycles if slow mem
sub addSlowCycles {
    my $reg = $_[1] || "%o4";
    my $slot = $_[2] || "nop";
    return
	"\tldub\t[MEMTYPE+$reg],%o7\n".
	"\tbtst\t0x$FAST_PATTERN,%o7\n".
	"\tbe\t1f\n"."\t$slot\n".
	"\taddcc\tCYCLES,$_[0],CYCLES\n".
	"1:\n";
}

sub getNextCycle($) {
    my $reg = $_[0];
    return
	"\tmovl\tEXTERN(next_timer_cycle), $reg\n".
	"\torb\t%bl, %bl\n".
	"\tcmovsl\tEXTERN(next_nmi_cycle), $reg\n";
}
sub callMethod($$) {
    my $method = $_[0];
    my $cleanup = $_[1];
    return
	"\tmovw\t%si,EXTERN(pc)\n".
	getNextCycle("%eax").
	"\taddl\t%eax,%ebp\n".
	"\tmovl\t%ebp,EXTERN(cycles)\n".
	"\tcall\t$method\n".
	$cleanup.
	getNextCycle("%ecx").
	"\tsubl\t%ecx,%ebp\n";
}


#### low level read / write

sub writeB() {
    return
	"\ttestb\t\$0x$WRITE_PATTERN, EXTERN(memtype)(%eax)\n".
	"\tjnz\tclean_up\n".
	"\tmovb\t%cl, EXTERN(memory)(%eax)\n";
}
sub writeW() {
    return
	"\ttestw\t\$0x$WRITE_PATTERN$WRITE_PATTERN, EXTERN(memtype)(%eax)\n".
	"\tjnz\tclean_up\n".
	"\tmovb\t%ch, EXTERN(memory)(%eax)\n".
	"\tmovb\t%cl, EXTERN(memory)+1(%eax)\n";
}
sub readB() {
    return
	"\ttestb\t\$0x$READ_PATTERN, EXTERN(memtype)(%eax)\n".
	"\tjnz\tclean_up\n".
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
	callMethod("*%ecx","").
	"\tpopl\t%eax\n".
	"\tpopl\t%eax\n".

	"5:\n";
}

sub readBport($) {
    $pattern = "0x$_[0]";
    $pattern2 = ($pattern & ~0x80);
    return
	"\tldub\t[MEMTYPE+%o4],%o7\n".
	"\tbtst\t$pattern,%o7\n".
	"\tbe,a\t5f\n".
	"\tldub\t[MEMORY+%o4],%o5\n".

	"\tbtst\t$pattern2,%o7\n".
	"\tbne\tclean_up\n".
	"\tmov\t0xff88,%o7\n".
	"\tsubcc\t%o4,%o7,%o4\n".
	"\tbcs\tclean_up\n".
	"\tsll\t%o4,3,%o4\n".
	"\tsethi\t%hi(EXTERN(port)),%o7\n".
	"\tadd\t%o4,%lo(EXTERN(port)),%o4\n".

	"\ttestb\t\$$pattern, EXTERN(memtype)(%eax)\n".
	"\tjz\t5f\n".

	"\ttestb\t\$$pattern2, EXTERN(memtype)(%eax)\n".
	"\tjnz\tclean_up\n".              # should we handle motor bits???
	"\tcmpl\t\$0xff88, %eax\n".
	"\tjb\tillegaladdr\n".
	"\tmovl\tEXTERN(port)-0x7fc40(,%eax,8),%edx\n".
	"\torl\t%edx,%edx\n".
	"\tjz\t5f\n".
	"\tpushl\t%eax\n".
	"\tpushl\t%ecx\n".
	callMethod("*%edx","").
	"\tmovb\t%al,%dl\n".
	"\tpopl\t%ecx\n".
	"\tpopl\t%eax\n".
	"\tmovb\t%dl, EXTERN(memory)(%eax)\n".

	"5:\n".
	"\tmovb\tEXTERN(memory)(%eax), %dl\n";   # ax = addr, dl = value
}

sub rwBport($$) {
    $code = $_[1];
    $pattern = "0x$_[0]";
    $pattern2 = sprintf "0x%02x", (hex($pattern) & ~0x80);
    $code2 = $code;
    $code2 =~ s/%o5/OPCSTAT/g;
    $code2 =~ s/$bitnr/OPCTABLE/g;
    return
	"\tldub\t[MEMTYPE+%o4],%o7\n".
	"\tbtst\t$pattern,%o7\n".
	"\tbe,a\t5f\n".
	"\tldub\t[MEMORY+%o4],$value\n".

	"\tbtst\t$pattern2,%o7\n".
	"\tbne\tclean_up\n".
	"\tbtst\t0x$FAST_PATTERN,%o7\n".
	"\tbeq,a\t1f\n".
	"\taddcc\tCYCLES,2,CYCLES\n".     # add 2 cycle if slow mem
	"1:\n".
	"\tand\t%o4,0xff,%o7\n".
	"\tsubcc\t%o7,0x88,%o7\n".
	"\tbcs\tclean_up\n".
	"\tsll\t%o7,3,%o7\n".
	"\tsethi\t%hi(EXTERN(port)),HI2CCR\n".
	"\tadd\tHI2CCR,%lo(EXTERN(port)),HI2CCR\n".
	"\tadd\tHI2CCR,%o7,HI2CCR\n".
	"\tld\t[HI2CCR],%o7\n".
	"\ttst\t%o7\n".
	"\tmov\t%o4,REG\n".
	"\tmov\t%o5,OPCSTAT\n".
	"\tmov\t$bitnr,OPCTABLE\n".
	"\tbeq,a\t4f\n".
	"\tldub\t[MEMORY+REG],$value\n".
	"\tcall\t%o7\n".
	"\tnop\n".
	"4:\n".
#	"\tcall\tEXTERN(logmemread)\n".
#	"\tmov\tREG,%o1\n".
	$code2.
#	"\tcall\tEXTERN(logmemwrite)\n".
#	"\tmov\tREG,%o1\n".
	"\tld\t[HI2CCR+4],%o7\n".
	"\ttst\t%o7\n".
	"\tbeq\t4f\n".
	"\tstb\t$value,[MEMORY+REG]\n".
	"\tcall\t%o7\n".
	"\tnop\n".
	"4:\n".
	"\tsethi\t%hi(EXTERN(reg)), REG\n".
	"\tsethi\t%hi(EXTERN(hi2ccr)+2), HI2CCR\n".
	"\tsethi\t%hi(EXTERN(opctable)), OPCTABLE\n".
	"\tsethi\t%hi(EXTERN(frame_asmopcstat)), OPCSTAT\n".
	"\tor\tREG, %lo(EXTERN(reg)), REG\n".
	"\tor\tHI2CCR, %lo(EXTERN(hi2ccr)+2), HI2CCR\n".
	"\tor\tOPCTABLE, %lo(EXTERN(opctable)), OPCTABLE\n".
	"\tb\t6f\n".
	"\tor\tOPCSTAT, %lo(EXTERN(frame_asmopcstat)), OPCSTAT\n".

	"5:\n".
	"\tbtst\t0x$FAST_PATTERN,%o7\n".
	"\tbeq,a\t1f\n".
	"\taddcc\tCYCLES,2,CYCLES\n".     # add 2 cycle if slow mem
	"1:\n".
	$code.
	"\tstb\t$value,[MEMORY+%o4]\n".
	"6:\n";
}

sub readW() {
    return
	"\ttestw\t\$0x$READ_PATTERN$READ_PATTERN, EXTERN(memtype)(%eax)\n".
	"\tjnz\tclean_up\n".
	"\tmovb\tEXTERN(memory)(%eax), %ch\n".
	"\tmovb\tEXTERN(memory)+1(%eax), %cl\n";
}

########  Misc instructions ###########################

sub Nop() {
    $goto="default_epilogue";
}

sub Sleep() {
    $goto="clean_up";
}

sub Trap() {
    $goto="trap";
}

######### Arithmetic instructions ########################

sub AddSubCommon($) {
    my ($ext, $shift, $x, $w, $h, $l, $mask, $add, $sub, $adc, $opc, $setRd);
    $opc = $_[0];

    
    $add = ($opc =~ /ADD/ ? "add" : "sub");
    $sub = ($opc =~ /ADD/ ? "sub" : "add");
    $ext   = $opc =~ /X/ ?  1 : 0;


    if ($opc =~ /W/) {
	$code =
	    "\taddl\t$destreg,REG,$destreg\n".
	    "\tldsb\t[$destreg],%o7\n".
	    "\tldub\t[$destreg+8],$destval\n".
	    "\tsll\t%o7,8,%o7\n".
	    "\tor\t%o7,$destval,$destval\n".
	    "\t${add}cc\t$destval,$value,$value\n";
	if ($opc !~ /CMP/) {
	    $code .= setWRs();
	}
	$code .= setNZVAC(16);

    } else {
	$code = 
	    "common_$opc:\n".
	    "\tldsb\t[REG+$destreg],$destval\n".
	    "\t${add}cc\t$destval,$value,$value\n";
	$code .= "\tstb\t$value,[REG+$destreg]\n" if ($opc !~ /CMP/);
	$code .= setNZVAC(8);
    }

    return $code;
}

sub AddBI() {
    return
	"\tsll\t$opcval,24,$value\n".
	"\tsra\t$value,24,$value\n".
	"\tand\t$opcode,15,$destreg\n".
	AddSubCommon("ADDB");
}
sub AddB() {
    return
	"\tand\t$opcval,15,$destreg\n".
	"\tsrl\t$opcval,4,$value\n".
	"\tb\tcommon_ADDB\n".
	"\tldsb\t[REG+$value],$value\n"
}
sub AddW() {
    $goto="clean_up"; return;
    return
	illOpc(0x88).
	"\tand\t$opcval,7,$destreg\n".
	getWRsNoMask().
	AddSubCommon("ADDW")
}
sub AddS() {    
    $goto="clean_up"; return;
    return
	illOpc(0x78).
	getWRdAddr().
	"\taddb\t%dl,%dl\n".
	"\tadcb\t\$1,EXTERN(reg)+8(%edi)\n".
	"\tadcb\t\$0,EXTERN(reg)(%edi)\n"
}
sub AddXI() {
    $goto="clean_up"; return;
    return
	AddSubCommon("ADDX")
}

sub AddX() {
    $goto="clean_up"; return;
    return
	"\tmovl\t%edx,%ecx\n".
	getBRs().
	AddSubCommon("ADDX")
}

sub CmpBI() {
    return
	"\tsll\t$opcval,24,$value\n".
	"\tsra\t$value,24,$value\n".
	"\tand\t$opcode,15,$destreg\n".
	AddSubCommon("CMPB");
}
sub CmpB() {
    return
	"\tand\t$opcval,15,$destreg\n".
	"\tsrl\t$opcval,4,$value\n".
	"\tb\tcommon_CMPB\n".
	"\tldsb\t[REG+$value],$value\n";
}
sub CmpW() {
    $goto="clean_up"; return;
    return
	illOpc(0x88).
	getWRd(). 
	getWRsNoMask().
	AddSubCommon("CMPW");
}

sub SubB() {
    return
	"\tand\t$opcval,15,$destreg\n".
	"\tsrl\t$opcval,4,$value\n".
	"\tldsb\t[REG+$value],$value\n".
	AddSubCommon("SUBB")
}
sub SubW() {
    $goto="clean_up"; return;
    return
	illOpc(0x88).
	getWRd(). 
	getWRsNoMask().
	AddSubCommon("SUBW")
}
sub SubS() {    
    $goto="clean_up"; return;
    return
	illOpc(0x78).
	getWRdAddr().
	"\taddb\t%dl,%dl\n".
	"\tsbbb\t\$1,EXTERN(reg)+8(%edi)\n".
	"\tsbbb\t\$0,EXTERN(reg)(%edi)\n"
}
sub SubXI() {
    $goto="clean_up"; return;
    return
	AddSubCommon("SUBX")
}

sub SubX() {
    $goto="clean_up"; return;
    return
	"\tmovl\t%edx,%ecx\n".
	getBRs().
	AddSubCommon("SUBX")
}

sub Inc() {
    return
	illOpc(0xf0).
	getBRd().

	"\taddcc\t$destval,1,$value\n".
	"\tstb\t$value,[REG+$destreg]\n".
	setNZV(8);
}
sub Dec() {
    return
	illOpc(0xf0).
	getBRd().

	"\tsubcc\t$destval,1,$value\n".
	"\tstb\t$value,[REG+$destreg]\n".
	setNZV(8);
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
    $extraopclen++;
    $extracycles+=4;
    return
	"\tsll\t$opcval,24,%o4\n".
	"\tsra\t%o4,8,%o4\n".
	"\tsrl\t%o4,16,%o4\n".
	"bitops1:\n".
	"\tadd\tPC,2,%o7\n".
	"\tlduh\t[MEMORY+%o7], $opcode\n".
	"\tsrl\t$opcode,5,%o5\n".
	"\tbtst\t0x0f,$opcode\n".
	"\tbne\tillegalOpcode\n".
	"\tsrl\t$opcode,4,$opcode\n".

	"\txor\t%o5,0x60<<3,%o5\n".
	"\tbtst\t0xec<<3,%o5\n".
	"\tbne\tclean_up\n".         # Can only be BSt which we don't handle
	"\tbtst\t0x10<<3,%o5\n".
	"\tbne\t1f\n".               # get shift count (register or immediate)
	"\tand\t$opcode,15,$bitnr\n".

	"\tldub\t[REG+$bitnr],$bitnr\n".
	"\tand\t$bitnr,7,$bitnr\n".
	"1:\n".
	"\tbtst\t8,$bitnr\n".
	"\tbne\tillegalOpcode\n".
	"\tand\t%o5,3<<3,%o5\n".
	"\txorcc\t%o5,3<<3,%o5\n".
        "\tbe\tillegalOpcode\n".

	rwBport($READWRITE_PATTERN,
		"\tcall\t1f\n".
		"\tmov\t1,%o2\n".
		
		"\tb\t2f\n".
		"\tbclr\t$bitnr,$value\n".      # clr
		
		"\tb\t2f\n".
		"\tbtog\t$bitnr,$value\n".      # not
		
		"\tb\t2f\n".
		"\tbset\t$bitnr,$value\n".      # set
		
		"1:\tjmp\t%o7+%o5\n".
		"\tsll\t%o2,$bitnr,$bitnr\n".
		"2:\n");
}

sub BitInd1() {
    $goto="clean_up"; return;
    $noepilogue=1;
    return
	illOpc(0x8f).
	"\tshrl\t\$4,%edx\n".
	"\txorl\t%eax,%eax\n".
	"\tmovb\tEXTERN(reg)+8(%edx),%al\n".
	"\tmovb\tEXTERN(reg)(%edx),%ah\n".
	"\tjmp\tbitops1\n";

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
    $goto="clean_up";
}

sub BClr() {
    $goto="clean_up";
}

sub BSetI() {
    $goto="clean_up";
}

sub BSet() {
    $goto="clean_up";
}

sub BNotI() {
    $goto="clean_up";
}
sub BNot() {
    $goto="clean_up";
}
sub BTstI() {
    $goto="clean_up"; return;
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
    $goto="clean_up"; return;
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
    $cyclesopclen = 1;
    $jmpslot = "add\tPC, $value, PC";
    return 
	"\tbtst\t1,$opcval\n".
	"do_bra:\n".
	"\tbne\tillegalOpcode\n".
	"\tsll\t$opcval, 24, $value\n".
        "\tsra\t$value, 24, $value\n";             # sign extend offset
}
sub BRN() {
    $goto="default_epilogue";
#    $extracycles = 2;
    "";
}
sub BCond($$) {
    $mask = $_[0];
    $test = $_[1];
    $cyclesopclen = 1;
    return "\tbtst\t$mask,CCR\n".
	"\tb$test\tdo_bra\n".
	"\tbtst\t1,$opcval\n";
}
sub BHI() {
    BCond(5,"e")
}
sub BLO() {
    BCond(5,"ne")
}
sub BCC() {
    BCond(1,"e")
}
sub BCS() {
    BCond(1,"ne")
}
sub BNE() {
    BCond(4,"e")
}
sub BEQ() {
    BCond(4,"ne")
}
sub BVC() {
    BCond(2,"e")
}
sub BVS() {
    BCond(2,"ne")
}
sub BPL() {
    BCond(8,"e")
}
sub BMI() {
    BCond(8,"ne")
}
sub BGE() {
#    $extracycles = 2;
    $goto="clean_up"; return;
    return "\tmovb\t%bl,%al\n".
	"\tshrb\t\$2,%al\n".
	"\txorb\t%bl,%al\n".
	"\tandb\t\$2,%al\n".
	"\tjz\tdo_bra\n";
}
sub BLT() {
#    $extracycles = 2;
    $goto="clean_up"; return;
    return "\tmovb\t%bl,%al\n".
	"\tshrb\t\$2,%al\n".
	"\txorb\t%bl,%al\n".
	"\tandb\t\$2,%al\n".
	"\tjnz\tdo_bra\n";
}
sub BGT() {
#    $extracycles = 2;
    $goto="clean_up"; return;
    return "\tmovb\t%bl,%al\n".
	"\tshrb\t\$2,%al\n".
	"\tandb\t\$2,%al\n".
	"\txorb\t%bl,%al\n".
	"\tandb\t\$6,%al\n".
	"\tjz\tdo_bra\n";
}
sub BLE() {
#    $extracycles = 2;
    $goto="clean_up"; return;
    return "\tmovb\t%bl,%al\n".
	"\tshrb\t\$2,%al\n".
	"\tandb\t\$2,%al\n".
	"\txorb\t%bl,%al\n".
	"\tandb\t\$6,%al\n".
	"\tjnz\tdo_bra\n";
}

sub JmpRI() {
    $goto="clean_up"; return;
    $noepilogue=1;
    return
	illOpc(0x8f).
	"\tshrl\t\$4,%edx\n".
	"\tmovb\tEXTERN(reg)(%edx),%ch\n".
	"\tmovb\tEXTERN(reg)+8(%edx),%cl\n".
	"\tjmp\tjmpcommon\n";
}

sub JmpCommon() {
    $goto="clean_up"; return;
    $extracycles = 4;   # read pc (either lookahead or normal)
    $extraopclen--;     # prevent increment of pc
    return
	"jmpcommon:\n".
	"\ttestl\t\$1,%ecx\n".
	"\tjnz\tunaligned\n".
	"\ttestb\t\$0x80,EXTERN(memtype)(%ecx)\n".
	"\tjnz\tunaligned\n".
	addSlowCycles(8, "%esi").            # add 2x4 cycles if slow mem
	"\tmovl\t%ecx,%esi\n";
}

sub JmpA16() {
    $goto="clean_up"; return;
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
    $goto="clean_up"; return;
    $extracycles = 0;      # cycles handled below
    $extraopclen--;        # prevent increment of pc
    $code =
	"\ttestl\t\$1, %eax\n".
	"\tjnz\tclean_up\n".
	"\tmovb\tEXTERN(reg)+7,%ch\n".
	"\tmovb\tEXTERN(reg)+8+7,%cl\n".
	"\tsubl\t\$2, %ecx\n".
	"\ttestw\t\$0x$WRITE_PATTERN$WRITE_PATTERN, EXTERN(memtype)(%ecx)\n".
	"\tjnz\tclean_up\n".
	"\tmovl\t%eax,%esi\n".
	"\tmovb\t%dh, EXTERN(memory)(%ecx)\n".
	"\tmovb\t%dl, EXTERN(memory)+1(%ecx)\n".
	"\tmovb\t%ch, EXTERN(reg)+7\n".
	"\tmovb\t%cl, EXTERN(reg)+8+7\n".
	"\taddl\t\$6,%ebp\n".
	addSlowCycles(8, "%edx").  # add 8 cycles if slow pc
	addSlowCycles(4, "%ecx").  # add 4 cycles if slow stack
	"\tpushl\t\$0\n".
	"\tpushl\t%ecx\n".
	callMethod("EXTERN(frame_begin)", "\taddl\t\$8, %esp\n");

    $extracycles = 0;
    return $code;
}

sub BSr() {
    $goto="clean_up"; return;
    $noepilogue=1;
    return
	"\tmovsbl\t%dl,%eax\n".
	"\tleal\t2(%esi),%edx\n".
	"\taddl\t%edx,%eax\n".
	"\tjmp\tjsrcommon\n";
}

sub JsrRI() {
    $goto="clean_up"; return;
    $noepilogue=1;
    return
	illOpc(0x8f).
	"\txorl\t%eax,%eax\n".
	getWRsNoMask().
	"\tleal\t2(%esi),%edx\n".
	"\tjmp\tjsrcommon\n";
}
sub JsrA16() {
    $goto="clean_up"; return;
    return
	illOpc(0xff).
	getWOpc.
	"\taddl\t\$2,%ebp\n".                  # internal cycles
	"\tleal\t4(%esi),%edx\n".
	"jsrcommon:\n".
	JsrCommon();
}

sub JsrAA8() {
    $goto="clean_up";
}


sub Return($) {
    $goto="clean_up"; return;
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
	"\tjnz\tclean_up\n".
	($rte ? "\taddl\t\$2, %ecx\n" : "").
	"\tpushl\t\$$rte\n".
	"\tpushl\t%ecx\n".
	callMethod("EXTERN(frame_end)", "\taddl\t\$8, %esp\n".
		   ($rte ? "\tmovb\tEXTERN(memory)(%edi),%bl\n": "")).
	"\txorl\t%ecx,%ecx\n".
	"\tmovb\tEXTERN(memory)".($rte?"+2":""  )."(%edi),%ch\n".
	"\tmovb\tEXTERN(memory)".($rte?"+3":"+1")."(%edi),%cl\n".
	"\ttestl\t\$1,%ecx\n".
	"\tjnz\tunaligned\n".
	"\ttestb\t\$0x80,EXTERN(memtype)(%ecx)\n".
	"\tjnz\tunaligned\n".
	"\tmovl\t%edi,%eax\n".
	"\taddl\t\$".($rte?"4":"2").",%eax\n".
	"\tmovb\t%ah,EXTERN(reg)+7\n".
	"\tmovb\t%al,EXTERN(reg)+8+7\n".
	addSlowCycles(8, "%esi").             # add 8 cycles if slow pc
	addSlowCycles($rte ? 8 : 4, "%edi"). # add 4/8 cycles if slow stack
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
	addSlowCycles(1).            # add 1 cycle if slow mem
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
	addSlowCycles(4).            # add 4 cycles if slow mem
	"\tandb\t\$0xf1,%bl\n".
	"\torw\t%cx,%cx\n".
	setNZ("MOVW");
}

sub EepMov() {
    $goto="clean_up";
}

sub MovB() {
    $goto="clean_up"; return;
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
    $goto="clean_up"; return;
    return
	"\tandb\t\$0xf1,%bl\n".
	"\tandl\t\$0x0f,%ecx\n".
	"\tmovb\t%dl,EXTERN(reg)(%ecx)\n".
	"\torb\t%dl,%dl\n".
	setNZ("MOVB");
}

sub MovBRI() {
    $goto="clean_up"; return;
    return
	getBRdAddr().
	getUWRs().
	MovBCore();
}
sub MovBRID() {
    $goto="clean_up"; return;
    $extraopclen++;
    return
	getWOpc().
	getBRdAddr().
	addUWRs().
	MovBCore();
}
sub MovBRIP() {
    $goto="clean_up"; return;
    return
	getBRdAddr().
	getUWRs().
	MovBCore("P")
}
sub MovBFA8() {
    $goto="clean_up"; return;
    $extracycles += 2;
    return
	"\tmovzbl\t%cl,%edi\n".
	"\tandl\t\$0x0f,%edi\n".
	"\tmovsbw\t%dl,%ax\n".
	"\tmovzwl\t%ax,%eax\n".

	readBport($READ_PATTERN).
	addSlowCycles(1).

	"\tmovb\t%dl,EXTERN(reg)(%edi)\n";
	"\tandb\t\$0xf1,%bl\n".
	"\torb\t%dl,%dl\n".
	setNZ("MOVB");
}
sub MovBTA8() {
    $goto="clean_up"; return;
    $extracycles += 2;
    return
	"\tmovzbl\t%cl,%edi\n".
	"\tandl\t\$0x0f,%edi\n".
	"\tmovsbw\t%dl,%ax\n".
	"\tmovzwl\t%ax,%ecx\n".
	
	"\ttestb\t\$0x$WRITE_PATTERN, EXTERN(memtype)(%ecx)\n".
	"\tjz\t5f\n".
	"\ttestb\t\$0x24, EXTERN(memtype)(%ecx)\n".
	"\tjnz\tclean_up\n".              # should we handle motor bits???
	"\tcmpl\t\$0xff88, %ecx\n".
	"\tjb\tillegaladdr\n".
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
    $goto="clean_up"; return;
    $extraopclen++;
    return
	illOpc(0x70).
	getWOpc().
	getBRdAddr().
	MovBCore("A16");
}

sub checkFrameSwitch() {
    $goto="clean_up"; return;
    return
	"\tcmpl\t\$7,%edi\n".
	"\tjz clean_up\n";
}

sub MovW() {
    $goto="clean_up"; return;
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
    $goto="clean_up"; return;
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
    $goto="clean_up"; return;
    return
	illOpc(0x08).
	getWRdAddr().
	checkFrameSwitch().
	getUWRs().
	MovWCore();
}
sub MovWRID() {
    $goto="clean_up"; return;
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
    $goto="clean_up"; return;
    return
	illOpc(0x08).
	getWRdAddr().
	checkFrameSwitch().
	getUWRs().
	MovWCore("P")
}
sub MovWA16() {
    $goto="clean_up"; return;
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
    $goto="clean_up"; return;
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
    $goto="clean_up"; return;
    illOpc(0x70).
	getBRdAddr().
	"\tandb\t\$0xf0,%bl\n".
	"\tshlb\t\$1,EXTERN(reg)(%edi)\n".     # do shift
	"\tseto\t%al\n".
	"\tlahf\n".                     # handle flags
	"\tshlb\t\$7,%dl\n".            # clear overflow if not SALL 
	"\tandb\t%dl,%al\n".
	"\trolw\t\$8,%ax\n".
	"\tandw\t\$~32,%ax\n".          # clear aux flag
	"\tmovzwl\t%ax,%eax\n".
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
    $goto="clean_up"; return;
    $op = $_[0];
    return
	"\tandb\t\$0xf1,%bl\n".
	"\tandl\t\$0x0f,%ecx\n".
	"\t\L$op\E\t%dl,EXTERN(reg)(%ecx)\n".
	setNZ($op);
}
sub Logic($) {
    $goto="clean_up"; return;
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
    $goto="clean_up"; return;
}


sub build_case($$) {
    my ($hex, $func) = @_;
    $noepilogue  = 0;
    $extraopclen = 0;
    $extracycles = 0;
    $cyclesopclen = 0;
    $goto = "opc$hex";
    $helper = "";
    $jmpslot = "nop";
    $case = &$func();

    $goto="clean_up" if 0;
    if ($hex =~ /x/) {
	$val = hex (substr($hex,0,1)."0");
	for ($i = 0; $i < 16; $i++) {
	    $opctable[$val+$i] = $goto;
	}
    } else {
	$opctable[hex $hex] = $goto;
    }

    return if ($goto ne "opc$hex");

    $opclen       = 2 + 2*$extraopclen;
    $cycles       = $opclen +  $extracycles + 2*$cyclesopclen;
    $slowcycles   = 2 * $opclen + 4*$cyclesopclen;

    if ($noepilogue) {
	$epilogue = "";
    } elsif(!$cyclesopclen && !$extracycles && !$extraopclen
	    && $jmpslot ne "nop") {
	$epilogue = "\tb\tdefault_epilogue\n"."\t".$jmpslot."\n";
    } elsif (!$extraopclen && !$cyclesopclen) {
	if ($cycles) {
	    $epilogue = ($jmpslot eq "nop" ? "" : "\t".$jmpslot."\n").
		"\tb\tdefault_epilogue_nocycles\n".
		"\taddcc\tCYCLES, $cycles, CYCLES\n";
	} else {
	    $epilogue = "\tb\tdefault_epilogue_nocycles\n".
		"\t".$jmpslot."\n";
	}
    } else {
	my $pre = ($jmpslot eq "nop") ? "" : "\t".$jmpslot."\n";
	$epilogue = 
	    ($opclen ?

	     $pre.
	     "\tldub\t[MEMTYPE+PC],%o5\n".
	     "\tadd\tPC,$opclen,PC\n".
	     "\tbtst\t0x$FAST_PATTERN,%o5\n".
	     "\tbne\tloop\n".
	     ($cycles ? "\taddcc\tCYCLES, $cycles, CYCLES\n" : "").
	     "\tb\tloop\n".
	     "\taddcc\tCYCLES,$slowcycles,CYCLES\n"
	     
	     : $cycles ?
	     $pre."\tb\tloop\n".
	     "\taddcc\tCYCLES, $cycles, CYCLES\n"
	     : "\tb\tloop\n\t$jmpslot\n");
    }

    return
	"! ($func)\n".
	"opc$hex:\n".
	$case.
	$epilogue.
	$helper;
}

@opctable = ("illegalOpcode") x 256;

print "\t.file\t\"h8300-sparc.inc\"\n";

while (<DATA>) {
    $_ =~ /([0-9a-fA-F][0-9a-fA-FxX])=(\w+)/ or next;
    $hex = $1;
    $func = $2;
    print build_case($hex, $func);
}

print "\t.section\t\".rodata\"\nopctable:\n";
print "\t.long\t$_\n" foreach (@opctable);

	   
print "\nhi2ccr:";
for ($i = -2; $i < 2; $i++) {
    if ($i == -2) {
	print "\n\t.byte\t";
    } else {
	print ", ";
    }
    my $ccr = 0;
    #$ccr |=  1 if ($i > 1 || ); # carry
    $ccr |=  2 if ($i < -1 || $i > 0); # overflow
    #$ccr |=  4 if ($i &  64); # zero
    $ccr |=  8 if ($i & 1); # sign
    #$ccr |= 32 if ($i &  16); # aux
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
