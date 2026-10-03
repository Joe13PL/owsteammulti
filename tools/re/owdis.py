"""Quick symbolized disassembler: python dis.py <exe> <tsv> name_or_hexva [...]"""
import sys,re,pefile,capstone
exe,tsv=sys.argv[1],sys.argv[2]
pe=pefile.PE(exe); base=pe.OPTIONAL_HEADER.ImageBase; img=pe.get_memory_mapped_image()
iat={i.address:e.dll.decode()+'!'+(i.name.decode() if i.name else str(i.ordinal)) for e in pe.DIRECTORY_ENTRY_IMPORT for i in e.imports}
names={};sizes={};byname={}
for l in open(tsv):
    c=l.rstrip('\n').split('\t'); va=int(c[0],16); names[va]=c[2]; sizes[va]=int(c[4]); byname.setdefault(c[2],va)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
def thunk(t):
    b=img[t-base:t-base+6]
    if b[:2]==b'\xff\x25': return iat.get(int.from_bytes(b[2:6],'little'))
def strat(v):
    try:
        o=v-base
        if 0<=o<len(img):
            # delphi AnsiString literal: length dword before
            ln=int.from_bytes(img[o-4:o],'little')
            if 0<ln<200: 
                s=img[o:o+ln]
                if all(32<=ch<127 for ch in s): return repr(s.decode())
    except Exception: pass
for a in sys.argv[3:]:
    va=int(a,16) if re.fullmatch(r'[0-9A-Fa-f]{6,8}',a) else byname[a]
    print('=====',names.get(va),hex(va))
    for ins in md.disasm(img[va-base:va-base+sizes.get(va,256)],va):
        s=f"{ins.address:08X} {ins.mnemonic} {ins.op_str}"
        for m in re.findall(r'0x[0-9a-f]+',ins.op_str):
            v=int(m,16)
            r=iat.get(v) or names.get(v) or (thunk(v) if ins.mnemonic in('call','jmp') else None) or strat(v)
            if r: s+='   ; '+r
        print(s)
