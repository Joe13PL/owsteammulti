"""Borland/Embarcadero TD32 (FB09) debug info parser for Delphi executables.

Extracts modules, procedures (incl. nested), locals/params, globals, line
numbers and the global type table. Output: JSON consumed by the Ghidra import
script and the source-tree generator.
"""
import json
import struct
import sys

import pefile

SST_MODULE, SST_ALIGNSYM, SST_SRCMODULE, SST_GLOBALTYPES, SST_NAMES = 0x120, 0x125, 0x127, 0x12B, 0x130

S_COMPILE, S_REGISTER, S_CONST, S_UDT, S_SSEARCH, S_END = 1, 2, 3, 4, 5, 6
S_OBJNAME = 9
S_BPREL32, S_LDATA32, S_GDATA32, S_PUB32, S_LPROC32, S_GPROC32 = 0x200, 0x201, 0x202, 0x203, 0x204, 0x205
S_THUNK32, S_BLOCK32, S_WITH32, S_LABEL32 = 0x206, 0x207, 0x208, 0x209
S_ENTRY32, S_OPTVAR32, S_PROCRET32, S_SAVREGS32 = 0x210, 0x211, 0x212, 0x213
S_SLINK32 = 0x230
S_USES = 0x24


class TD32:
    def __init__(self, path):
        self.pe = pefile.PE(path, fast_load=True)
        raw = open(path, 'rb').read()
        self.base_va = self.pe.OPTIONAL_HEADER.ImageBase
        self.sections = [s for s in self.pe.sections]
        dbg = None
        for s in self.sections:
            if s.Name.startswith(b'.debug'):
                dbg = raw[s.PointerToRawData:s.PointerToRawData + s.SizeOfRawData]
        if dbg is None:
            # TD32 appended as overlay (classic layout: FBxx ... FBxx + size at EOF)
            sig, size = raw[-8:-4], struct.unpack('<I', raw[-4:])[0]
            dbg = raw[len(raw) - size:]
        off = dbg.find(b'FB09')
        if off < 0:
            off = dbg.find(b'FB0A')
        self.B = dbg[off:]
        self._parse_dir()
        self._parse_names()

    # ---------------------------------------------------------------- basics
    def _parse_dir(self):
        B = self.B
        lfo = struct.unpack_from('<I', B, 4)[0]
        cbh, cbe, cdir = struct.unpack_from('<HHI', B, lfo)
        self.dir = []
        for i in range(cdir):
            st, im, lo, cb = struct.unpack_from('<HHII', B, lfo + cbh + i * cbe)
            self.dir.append((st, im, lo, cb))

    def _parse_names(self):
        st, im, lo, cb = [e for e in self.dir if e[0] == SST_NAMES][0]
        B = self.B
        n = struct.unpack_from('<I', B, lo)[0]
        p = lo + 4
        self.names = [None]
        for _ in range(n):
            ln = B[p]
            self.names.append(B[p + 1:p + 1 + ln].decode('latin-1'))
            p += ln + 2

    def name(self, idx):
        if 0 < idx < len(self.names):
            return self.names[idx]
        return None

    def seg_va(self, seg, off):
        if 1 <= seg <= len(self.sections):
            return self.base_va + self.sections[seg - 1].VirtualAddress + off
        return None

    # ---------------------------------------------------------------- modules
    def modules(self):
        mods = {}
        B = self.B
        for st, im, lo, cb in self.dir:
            if st != SST_MODULE:
                continue
            ovl, lib, cseg, style, nidx = struct.unpack_from('<HHHHI', B, lo)
            segs = []
            p = lo + 0x1C
            for _ in range(cseg):
                sidx, fl, so, ssz = struct.unpack_from('<HHII', B, p)
                segs.append({'seg': sidx, 'flags': fl, 'va': self.seg_va(sidx, so), 'size': ssz})
                p += 12
            mods[im] = {'index': im, 'name': self.name(nidx), 'segments': segs}
        return mods

    # ---------------------------------------------------------------- symbols
    def symbols(self, imod, lo, cb):
        B = self.B
        p = lo + 4  # skip signature dword
        end = lo + cb
        out = []
        stack = []
        while p < end:
            ln, typ = struct.unpack_from('<HH', B, p)
            if ln == 0:
                break
            rec = B[p + 4:p + 2 + ln]
            r = {'kind': typ, 'off': p - lo}
            if typ in (S_LPROC32, S_GPROC32):
                (parent, pend, pnext, size, dbs, dbe, off, seg, ptype, nearfar, _res, nidx) = struct.unpack_from('<IIIIIIIHIBBI', rec, 0)
                r.update(k='proc', glob=typ == S_GPROC32, va=self.seg_va(seg, off), size=size, dbg_start=dbs,
                         dbg_end=dbe, type=ptype, name=self.name(nidx), parent=parent, locals=[], depth=len(stack))
                if len(rec) >= 0x2B:
                    lname = struct.unpack_from('<I', rec, 0x27)[0]
                    if lname:
                        r['link_name'] = self.name(lname)
                stack.append(r)
                out.append(r)
                p += ln + 2
                continue
            if typ == S_END:
                if stack:
                    stack.pop()
                p += ln + 2
                continue
            if typ == S_BPREL32:
                off, t, nidx = struct.unpack_from('<iII', rec, 0)
                v = {'k': 'bprel', 'ebp': off, 'type': t, 'name': self.name(nidx)}
                if stack:
                    stack[-1]['locals'].append(v)
            elif typ == S_REGISTER:
                t, reg, nidx = struct.unpack_from('<IHI', rec, 0)
                v = {'k': 'reg', 'reg': reg, 'type': t, 'name': self.name(nidx)}
                if stack:
                    stack[-1]['locals'].append(v)
            elif typ in (S_LDATA32, S_GDATA32, S_PUB32):
                off, seg, _f, t, nidx = struct.unpack_from('<IHHII', rec, 0)
                v = {'k': 'data', 'glob': typ != S_LDATA32, 'va': self.seg_va(seg, off), 'type': t,
                     'name': self.name(nidx), 'imod': imod}
                if stack:
                    stack[-1]['locals'].append(v)
                else:
                    out.append(v)
            elif typ == S_CONST:
                t, nidx = struct.unpack_from('<II', rec, 0)
                v = {'k': 'const', 'type': t, 'name': self.name(nidx)}
                (stack[-1]['locals'] if stack else out).append(v)
            elif typ == S_UDT:
                t, _f, nidx = struct.unpack_from('<IHI', rec, 0)
                v = {'k': 'udt', 'type': t, 'name': self.name(nidx), 'imod': imod}
                (stack[-1]['locals'] if stack else out).append(v)
            elif typ == S_LABEL32:
                off, seg, _nf, nidx = struct.unpack_from('<IHBI', rec, 0)[:4] if len(rec) >= 11 else (0, 0, 0, 0)
                v = {'k': 'label', 'va': self.seg_va(seg, off), 'name': self.name(nidx)}
                if stack:
                    stack[-1]['locals'].append(v)
            p += ln + 2
        return out

    # ---------------------------------------------------------------- lines
    def lines(self, lo, cb):
        B = self.B
        cfile, cseg = struct.unpack_from('<HH', B, lo)
        fbase = struct.unpack_from('<%dI' % cfile, B, lo + 4)
        files = []
        for fb in fbase:
            p = lo + fb
            # Borland layout: cSeg u16, nameIndex u32, baseSrcLn[cSeg] u32, (start,end)[cSeg]
            fcseg, nidx = struct.unpack_from('<HI', B, p)
            lnbase = struct.unpack_from('<%dI' % fcseg, B, p + 6)
            fname = self.name(nidx)
            entries = []
            for lb in lnbase:
                r = lo + lb
                seg, cpair = struct.unpack_from('<HH', B, r)
                offs = struct.unpack_from('<%dI' % cpair, B, r + 4)
                lns = struct.unpack_from('<%dH' % cpair, B, r + 4 + 4 * cpair)
                for o, l in zip(offs, lns):
                    entries.append((self.seg_va(seg, o), l))
            files.append({'file': fname, 'lines': entries})
        return files

    # ---------------------------------------------------------------- types
    def raw_types(self):
        st, im, lo, cb = [e for e in self.dir if e[0] == SST_GLOBALTYPES][0]
        B = self.B
        flags, count = struct.unpack_from('<II', B, lo)
        offs = struct.unpack_from('<%dI' % count, B, lo + 8)
        base = lo + 8 + 4 * count
        types = {}
        for i, o in enumerate(offs):
            p = base + o
            ln, leaf = struct.unpack_from('<HH', B, p)
            types[0x1000 + i] = (leaf, B[p + 4:p + 2 + ln])
        return types

    def run(self):
        mods = self.modules()
        procs, globs, udts = [], [], []
        lines = {}
        for st, im, lo, cb in self.dir:
            if st == SST_ALIGNSYM:
                mname = mods.get(im, {}).get('name')
                for r in self.symbols(im, lo, cb):
                    r['module'] = mname
                    if r.get('k') == 'proc':
                        procs.append(r)
                    elif r.get('k') == 'data':
                        globs.append(r)
                    elif r.get('k') == 'udt':
                        udts.append(r)
            elif st == SST_SRCMODULE:
                mname = mods.get(im, {}).get('name')
                lines[mname] = self.lines(lo, cb)
        return {'modules': list(mods.values()), 'procs': procs, 'globals': globs, 'udts': udts, 'lines': lines}


if __name__ == '__main__':
    t = TD32(sys.argv[1])
    res = t.run()
    with open(sys.argv[2], 'w') as fh:
        json.dump(res, fh)
    print('modules', len(res['modules']), 'procs', len(res['procs']), 'globals', len(res['globals']),
          'udts', len(res['udts']), 'line files', sum(len(v) for v in res['lines'].values()))
