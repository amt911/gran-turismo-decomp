import re,glob,sys
sys.path.insert(0,'/home/andres/repos/gt-synth')
from pathlib import Path
from tools.progress import load_functions, decompiled_functions
B=0x08804000
F=load_functions(Path('/home/andres/repos/gran-turismo-decomp/recomp/analysis/functions.tsv'))
F={a+B:s for a,s in F.items()}
D={a+B for a in decompiled_functions(Path('/home/andres/repos/gt-synth/src'))}
R=set()
for p in glob.glob('/home/andres/repos/gran-turismo-decomp/recomp/analysis/probe_pbase_*.log'):
    for m in re.finditer(r'\[probe\] 0x([0-9A-F]{8}) hits=(\d+)',open(p,errors='replace').read()):
        if int(m.group(2))>0: R.add(int(m.group(1),16))
U=set(F)-D
print(len(F),len(D),len(U),len(U&R),len(U-R), len(D-R))
