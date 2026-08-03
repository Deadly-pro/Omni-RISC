#!/usr/bin/env python3
"""gpu_asm.py — assembler for the Omni-RISC GPU 16-bit SIMT ISA.

Encoding: [15:12]op [11:9]rd [8:6]rs1 [5:3]rs2 [2:0]f3
  0=ALU(f3: ADD SUB AND OR XOR SLT SLTU SLL)   4=ALU2(f3: SRL SRA MUL)
  1=LSU(bit0: 0=LD rd<-sp[rs1], 1=ST sp[rs1]<-rs2)
  2=BR(instr[7:0]=byte target)   5=LDI(rd<-signext imm9)   F=HALT

Usage: gpu_asm.py kernel.S [-o kernel.hex]
Output: one 4-digit hex word per line ($readmemh / fscanf friendly).
"""
import re, sys, argparse

ALU  = {'ADD':0,'SUB':1,'AND':2,'OR':3,'XOR':4,'SLT':5,'SLTU':6,'SLL':7}
ALU2 = {'SRL':0,'SRA':1,'MUL':2}

def reg(tok):
    m = re.fullmatch(r'r([0-7])', tok, re.I)
    if not m: raise ValueError(f"bad register '{tok}'")
    return int(m.group(1))

def imm(tok):
    v = int(tok, 0)
    if not -256 <= v <= 511: raise ValueError(f"imm9 out of range: {v}")
    return v & 0x1FF

def assemble(lines):
    words, labels, insns = [], {}, []
    # pass 1: strip comments, collect labels (2 bytes per insn)
    for ln in lines:
        ln = re.sub(r'[#;].*', '', ln).strip()
        if not ln: continue
        m = re.match(r'(\w+):\s*(.*)', ln)
        if m:
            labels[m.group(1)] = len(insns) * 2
            ln = m.group(2).strip()
            if not ln: continue
        insns.append(ln)
    # pass 2: encode
    for ln in insns:
        parts = re.split(r'[,\s]+', ln)
        op, args = parts[0].upper(), parts[1:]
        if op in ALU:
            w = (0 << 12) | (reg(args[0]) << 9) | (reg(args[1]) << 6) | (reg(args[2]) << 3) | ALU[op]
        elif op in ALU2:
            w = (4 << 12) | (reg(args[0]) << 9) | (reg(args[1]) << 6) | (reg(args[2]) << 3) | ALU2[op]
        elif op == 'LDI':
            w = (5 << 12) | (reg(args[0]) << 9) | imm(args[1])
        elif op == 'LD':
            w = (1 << 12) | (reg(args[0]) << 9) | (reg(args[1]) << 6)
        elif op == 'ST':
            w = (1 << 12) | (reg(args[0]) << 6) | (reg(args[1]) << 3) | 1
        elif op == 'BR':
            t = labels[args[0]] if args[0] in labels else int(args[0], 0)
            w = (2 << 12) | (t & 0xFF)
        elif op == 'HALT':
            w = 0xF000
        else:
            raise ValueError(f"unknown mnemonic '{op}'")
        words.append(w)
    return words

if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('src')
    p.add_argument('-o', '--out')
    a = p.parse_args()
    out = a.out or re.sub(r'\.S$', '', a.src) + '.hex'
    with open(a.src) as f:
        words = assemble(f.readlines())
    with open(out, 'w') as f:
        f.write('\n'.join(f'{w:04x}' for w in words) + '\n')
    print(f"{a.src} -> {out}: {len(words)} instructions")
