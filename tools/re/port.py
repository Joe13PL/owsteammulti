"""Port function addresses from the TD32 debug build to another build via EXEMAP (unit+line)."""
import os,sys,json; sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from exemap import ExeMap
_j=None
def td32():
    global _j
    if _j is None: _j=json.load(open(os.environ.get('TD32_JSON','td32_sgui_debug.json')))
    return _j
def first_line(va,unit):
    for fl in td32()['lines'].get(unit,[]):
        for a,l in fl['lines']:
            if a==va: return l
def port(target_map,va,unit):
    l=first_line(va,unit)
    return None if l is None else target_map.addr(unit,l)
