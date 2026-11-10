import sys,glob,collections,pickle
sys.path.insert(0,'/tmp/claude-1000/synth')
from base import *
st=pickle.load(open('/tmp/claude-1000/synth/static.pkl','rb')); runs=st['runs']
Ur=U-R
# runtime-address runs from the scan files (V lines) -> function lists via static runs (same thing). Map run start (runtime) -> members
runrt=[(a+B,[f+B for f in c]) for a,c in runs]
objs=collections.defaultdict(lambda: collections.defaultdict(set)) # route -> vptr -> objects
allobj=collections.defaultdict(set); end=collections.defaultdict(set)
for p in sorted(glob.glob('/home/andres/repos/gt-synth/recomp/analysis/spike/scan_*.txt')):
    route=p.split('scan_')[1][:-4]
    for line in open(p):
        if line[0]=='O':
            _,o,v=line.split(); allobj[route].add(int(v,16))
vptrs_by_route=allobj
# function -> class instance seen? vptr in [start, start+4n)
import bisect
def classes_for(vset):
    hit=set()
    for start,mem in runrt:
        if any(start<=v<start+4*len(mem) for v in vset): hit.update(mem)
    return hit
per={r:classes_for(v) for r,v in vptrs_by_route.items()}
union=set().union(*per.values())
inrun_all={f for s,m in runrt for f in m}
print('routes',{r:len(v) for r,v in vptrs_by_route.items()})
print('distinct vptrs total',len(set().union(*vptrs_by_route.values())))
print('U unreached in a run',len(Ur&inrun_all),' with a live instance of its class (any route, any time)',len(Ur&union))
for r,h in per.items(): print(' ',r,len(Ur&h))
print('decompiled-unreached in run',len((D-R)&inrun_all),' with live instance',len((D-R)&union))
print('U reached',len(U&R),' (direct)')
lst=sorted((D-R)&union)
open('/tmp/claude-1000/synth/inst160.txt','w').write(','.join('0x%08X'%f for f in lst))
print(len(lst))
import re
def parse(p):
    out={}
    for l in open(p,errors='replace'):
        m=re.search(r'@0x([0-9A-F]{8}): compared (\d+), equal (\d+), mismatched (\d+), not comparable (\d+)',l)
        if m: out[int(m.group(1),16)]=tuple(map(int,m.groups()[1:]))
    return out
base=parse('/home/andres/repos/gt-synth/recomp/analysis/spike/full_base.log'); cls=parse('/home/andres/repos/gt-synth/recomp/analysis/spike/full_class.log')
print('parsed',len(base),len(cls))
def cnt(S,d): return sum(1 for f in S if f in d and d[f][1]>0)
groups={'decompiled & route-reached (D&R)':D&R,'decompiled & unreached (D-R)':D-R,'  of which in vtable run':(D-R)&inrun_all,'  of which not in run':(D-R)-inrun_all}
for n,S in groups.items():
    print(n,len(S),'base>=1equal',cnt(S,base),'class>=1equal',cnt(S,cls),'mismatch base/class',sum(1 for f in S if f in base and base[f][2]>0),sum(1 for f in S if f in cls and cls[f][2]>0))
print('---- by size, informative')
def parse2(p):
    out={}
    for l in open(p,errors='replace'):
        m=re.search(r'@0x([0-9A-F]{8}): compared (\d+), equal (\d+), mismatched (\d+), not comparable (\d+) \(calls \d+, cases that changed memory (\d+)',l)
        if m: out[int(m.group(1),16)]=tuple(map(int,m.groups()[1:]))
    return out
b2=parse2('/home/andres/repos/gt-synth/recomp/analysis/spike/full_base.log'); c2=parse2('/home/andres/repos/gt-synth/recomp/analysis/spike/full_class.log')
Fs={f:s for f,s in F.items()}
for lo,hi in ((0,100),(100,300),(300,1000),(1000,10**9)):
    S={f for f in D-R if lo<=Fs.get(f,0)<hi}
    e=lambda d: sum(1 for f in S if f in d and d[f][1]>0)
    return_=lambda d: sum(1 for f in S if f in d and d[f][1]>0 and d[f][4]>0)
    print('size',lo,hi,'n',len(S),'equal>=1 base',e(b2),'class',e(c2),' +changed-memory base',return_(b2),'class',return_(c2))
Sdyn={f for f in U}
print('remaining-to-decompile by size', {k:sum(1 for f in U if lo<=Fs.get(f,0)<hi) for k,(lo,hi) in {'<100':(0,100),'100-300':(100,300),'300-1000':(300,1000),'>=1000':(1000,10**9)}.items()})
print('remaining unreached by size', {k:sum(1 for f in Ur if lo<=Fs.get(f,0)<hi) for k,(lo,hi) in {'<100':(0,100),'100-300':(100,300),'300-1000':(300,1000),'>=1000':(1000,10**9)}.items()})
