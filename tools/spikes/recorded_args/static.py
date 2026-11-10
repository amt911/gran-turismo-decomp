import struct,sys,collections
sys.path.insert(0,'/home/andres/repos/gt-synth'); sys.path.insert(0,'/tmp/claude-1000/synth')
from base import *
from tools.family_members import TextImage,_sections
elf=open('/home/andres/repos/gt-synth/extracted/EBOOT.elf','rb').read()
img=TextImage.from_elf(elf)
Fe={a-B:s for a,s in F.items()}
starts=sorted(Fe)
# call edges
import bisect
edges=collections.defaultdict(set); callers=collections.defaultdict(set)
for fa,sz in Fe.items():
    try: words,rel=img.function(fa,sz)
    except Exception: continue
    for i,w in enumerate(words):
        if (w>>26) in (3,) and i in rel and (rel[i]&0xff)==4:
            t=(w&0x3ffffff)<<2
            if t in Fe: edges[fa].add(t); callers[t].add(fa)
print('edges',sum(len(v) for v in edges.values()))
# vtable runs in data sections
runs=[]
for name,s in _sections(elf):
    typ,flags,addr,off,size=s[1],s[2],s[3],s[4],s[5]
    if typ!=1 or name.startswith(b'.text') or not name.startswith((b'.data',b'.rodata')): continue
    data=elf[off:off+size]
    cur=[]
    for k in range(0,len(data)-3,4):
        w=struct.unpack_from('<I',data,k)[0]
        if w in Fe: cur.append(w)
        else:
            if len(cur)>=2: runs.append((addr+k-4*len(cur),cur))
            cur=[]
    if len(cur)>=2: runs.append((addr+len(data)-4*len(cur),cur))
print('vtable-like runs',len(runs))
inrun=collections.defaultdict(list)
for ri,(a,c) in enumerate(runs):
    for f in c: inrun[f+B].append(ri)
Ur=U-R
print('U unreached',len(Ur),'in a vtable run',sum(1 for f in Ur if f in inrun))
print('U unreached whose run has a reached sibling',sum(1 for f in Ur if any(any((g+B) in R for g in runs[ri][1]) for ri in inrun.get(f,()))))
print('U unreached with reached caller',sum(1 for f in Ur if any((c+B) in R for c in callers.get(f-B,()))))
print('U unreached with any caller',sum(1 for f in Ur if callers.get(f-B)))
# unreached with no caller and not in vtable
print('no caller, no vtable',sum(1 for f in Ur if not callers.get(f-B) and f not in inrun))
# same-TU neighbour: nearest reached function within 0x400 bytes before (call family by address)
rs=sorted(r for r in R)
def near(f,d=0x300):
    i=bisect.bisect_left(rs,f); 
    return any(abs(rs[j]-f)<=d for j in (i-1,i) if 0<=j<len(rs))
print('U unreached with reached function within 0x300',sum(1 for f in Ur if near(f)))
import pickle; pickle.dump(dict(runs=runs,callers={k:set(v) for k,v in callers.items()}),open('/tmp/claude-1000/synth/static.pkl','wb'))

# function-pointer refs anywhere in data (any run length)
refs=collections.Counter()
for name,s_ in _sections(elf):
    typ,addr,off,size=s_[1],s_[3],s_[4],s_[5]
    if typ!=1 or name.startswith(b'.text') or not name.startswith((b'.data',b'.rodata')): continue
    data=elf[off:off+size]
    for k in range(0,len(data)-3,4):
        w=struct.unpack_from('<I',data,k)[0]
        if w in Fe: refs[w+B]+=1
print('U unreached referenced from data',sum(1 for f in Ur if refs[f]),' and neither caller nor data ref',sum(1 for f in Ur if not refs[f] and not callers.get(f-B)))
taken=collections.defaultdict(set)
for fa,sz in Fe.items():
    try: words,rel=img.function(fa,sz)
    except Exception: continue
    for i,w in enumerate(words):
        if (w>>26)==15 and i in rel and (rel[i]&0xff)==5:
            rt=(w>>16)&31
            for j in range(i+1,min(i+8,len(words))):
                v=words[j]
                if (v>>26)==9 and j in rel and (rel[j]&0xff)==6 and ((v>>21)&31)==rt:
                    lo=v&0xffff; lo-=0x10000 if lo&0x8000 else 0
                    t=((w&0xffff)<<16)+lo
                    if t in Fe: taken[t].add(fa)
print('U unreached address-taken in code',sum(1 for f in Ur if taken.get(f-B)))
print('none of caller/dataref/taken',sum(1 for f in Ur if not refs[f] and not callers.get(f-B) and not taken.get(f-B)))
print('address-taken by a reached function',sum(1 for f in Ur if any((t+B) in R for t in taken.get(f-B,()))))
pickle.dump(dict(taken={k:set(v) for k,v in taken.items()},refs=dict(refs)),open('/tmp/claude-1000/synth/static2.pkl','wb'))
DR=D-R
inr=[f for f in DR if f in inrun]
print('decompiled-unreached in vtable run',len(inr),' decompiled-reached in run',sum(1 for f in D&R if f in inrun))
import random
random.seed(5); smp=sorted(random.sample(inr,min(400,len(inr))))
open('/tmp/claude-1000/synth/sample.txt','w').write(','.join('0x%08X'%f for f in smp))
sib={f for f in Ur if any(any((g+B) in R for g in runs[ri][1]) for ri in inrun.get(f,()))}
rc={f for f in Ur if any((c+B) in R for c in callers.get(f-B,()))}
tk={f for f in Ur if any((t+B) in R for t in taken.get(f-B,()))}
# 2-hop: caller (any) of which a caller is reached
rc2={f for f in Ur if f not in rc and any(any((cc+B) in R for cc in callers.get(c,())) for c in callers.get(f-B,()))}
print('donors: sibling',len(sib),'reached-caller',len(rc),'taken-by-reached',len(tk),'union',len(sib|rc|tk),'+2hop',len((sib|rc|tk|rc2)),'rc2',len(rc2))
Rr=R-{0x0880410C}
noref=[f for f in Rr if f in F and not refs[f] and not callers.get(f-B) and not taken.get(f-B)]
print('CALIB reached functions',len(Rr),'with no static ref',len(noref))
