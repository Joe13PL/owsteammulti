"""Bulk direct-call xrefs: python xrefs.py exe tsv regex_of_target_names -> target <- caller (unit)"""
import sys,struct,re,bisect,collections,pefile
exe,tsv,rx=sys.argv[1],sys.argv[2],re.compile(sys.argv[3])
pe=pefile.PE(exe,fast_load=True); base=pe.OPTIONAL_HEADER.ImageBase; img=pe.get_memory_mapped_image()
rows=[];unit={};tg={}
for l in open(tsv):
    c=l.rstrip('\n').split('\t')
    if c[1]!='P': continue
    va=int(c[0],16); rows.append((va,c[2])); unit[va]=c[3]
    if rx.search(c[2]): tg[va]=c[2]
rows.sort(); vas=[r[0] for r in rows]
code=pe.sections[0]; s=code.VirtualAddress; e=s+code.Misc_VirtualSize
out=collections.defaultdict(collections.Counter)
for i in range(s,e-5):
    if img[i]==0xE8:
        t=base+i+5+struct.unpack_from('<i',img,i+1)[0]
        if t in tg:
            k=bisect.bisect_right(vas,base+i)-1
            out[tg[t]][rows[k][1]]+=1
for t in sorted(out):
    print('==',t, sum(out[t].values()))
    for c,n in out[t].most_common(): print('   ',n,c)
