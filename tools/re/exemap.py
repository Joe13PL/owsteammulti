"""Parser for the EXEMAP overlay (unit table + address/line/unit triples) of OW Delphi builds."""
import struct,pefile,bisect
class ExeMap:
    def __init__(self,path):
        pe=pefile.PE(path,fast_load=True); d=open(path,'rb').read()
        o=pe.get_overlay_data_start_offset()
        nunits,nent=struct.unpack_from('<II',d,o); p=o+8
        self.units=[];self.segs=[]
        for i in range(nunits):  # record: ShortString[255] name + 3 x (start,size)
            ln=d[p]; self.units.append(d[p+1:p+1+ln].decode('latin-1'))
            self.segs.append([struct.unpack_from('<II',d,p+256+8*k) for k in range(3)]); p+=280
        self.entries=[struct.unpack_from('<III',d,p+12*i) for i in range(nent)]  # (addr,line,unit)
        self.by_unit_line={}
        for a,l,u in self.entries:
            self.by_unit_line.setdefault((self.units[u].lower(),l),a)
        self.addrs=sorted(e[0] for e in self.entries)
    def addr(self,unit,line): return self.by_unit_line.get((unit.lower(),line))
