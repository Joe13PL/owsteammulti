"""Call-graph reachability over direct calls: python reach.py exe tsv src_regex dst_regex"""
import sys,struct,re,bisect,collections,pefile
exe,tsv,srx,drx=sys.argv[1],sys.argv[2],re.compile(sys.argv[3]),re.compile(sys.argv[4])
pe=pefile.PE(exe,fast_load=True); base=pe.OPTIONAL_HEADER.ImageBase; img=pe.get_memory_mapped_image()
rows=[]
for l in open(tsv):
    c=l.rstrip('\n').split('\t')
    if c[1]=='P': rows.append((int(c[0],16),c[2],int(c[4])))
rows.sort(); vas=[r[0] for r in rows]; name={r[0]:r[1] for r in rows}
g=collections.defaultdict(set)
for va,n,sz in rows:
    o=va-base
    for i in range(o,o+sz-4):
        if img[i]==0xE8:
            t=base+i+5+struct.unpack_from('<i',img,i+1)[0]
            if t in name: g[va].add(t)
src=[r[0] for r in rows if srx.search(r[1])]
for s in src:
    prev={s:None}; q=collections.deque([s]); hits=[]
    while q:
        u=q.popleft()
        if drx.search(name[u]) and u!=s: hits.append(u); continue
        for v in g[u]:
            if v not in prev: prev[v]=u; q.append(v)
    print('##',name[s],'reaches',len(hits))
    for h in hits[:15]:
        path=[];x=h
        while x is not None: path.append(name[x]); x=prev[x]
        print('   ',' <- '.join(path[:7]))
