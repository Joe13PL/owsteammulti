"""Find direct call/jmp xrefs (E8/E9 rel32) to a target: python xref.py exe tsv target"""
import sys,struct,pefile,bisect
exe,tsv,t=sys.argv[1],sys.argv[2],sys.argv[3]
pe=pefile.PE(exe); base=pe.OPTIONAL_HEADER.ImageBase; img=pe.get_memory_mapped_image()
rows=[]
byname={}
for l in open(tsv):
    c=l.rstrip('\n').split('\t')
    if c[1]=='P': rows.append((int(c[0],16),c[2])); byname[c[2]]=int(c[0],16)
rows.sort(); vas=[r[0] for r in rows]
tgt=byname.get(t) or int(t,16)
code=pe.sections[0]; s=code.VirtualAddress; e=s+code.Misc_VirtualSize
for i in range(s,e-5):
    if img[i] in (0xE8,0xE9):
        d=struct.unpack_from('<i',img,i+1)[0]
        if base+i+5+d==tgt:
            va=base+i; k=bisect.bisect_right(vas,va)-1
            print(hex(va), rows[k][1] if k>=0 else '?')
