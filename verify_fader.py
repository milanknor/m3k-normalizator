#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Overeni chovani normalizace (fader): replikuje gain logiku z processBlock.
Scenar: ticho -> hlasitost -26 LUFS -> mezera (ticho) -> znovu -26 LUFS.
Ocekavame: gain vyjede na ~+12 dB (cil -14), v mezere spadne k 0 (drzeni cutu /
relaxace), po mezere znovu nabehne. (To je presne to "padani" v grafu = mezera.)
"""
import math
FS=48000.0; BLK=512
target=-14.0; downMs=80.0; upBaseMs=300.0
warmCoeff=math.exp(-BLK/(FS*0.005))
downCoeff=math.exp(-BLK/(FS*downMs/1000.0))
momSamples=int(0.4*FS); intMin=int(0.4*FS)
ST_N=int(3.0*FS/BLK)

def run(mode):
    segs=[(1.0,None),(6.0,-26.0),(1.5,None),(6.0,-26.0)]
    g=1.0; intSum=0.0; intCount=0; st=[]; act=0; sil=0
    p=lambda L:10**(L/10.0)
    timeline=[]; t=0.0
    for dur,L in segs:
        for _ in range(int(dur*FS/BLK)):
            t+=BLK/FS; signal=(L is not None); inst=L if signal else -200.0
            if signal: act+=BLK; sil=0
            else: act=0; sil+=BLK
            if sil>FS*0.2: intSum=0.0; intCount=0
            if signal: intSum+=p(L)*BLK; intCount+=BLK
            intL=(10*math.log10(intSum/intCount)) if intCount>0 else -70.0
            st.append(p(L) if signal else 0.0)
            if len(st)>ST_N: st.pop(0)
            ssum=sum(st)
            stL=(10*math.log10(ssum/len(st))) if ssum>0 else -70.0
            ref = stL if mode=='Short' else intL
            windowNeeded = ST_N*BLK if mode=='Short' else int(0.4*FS)
            tgtG=1.0; cutOnly=False
            if signal:
                r=ref
                if act<windowNeeded: r=inst; cutOnly=True
                if r>-69.0:
                    diff=target-r
                    allow=(not cutOnly) and act>=momSamples
                    if not allow: diff=min(0.0,diff)
                    elif diff>0:
                        if intCount>intMin and intL>-69.0: diff=min(diff,target-intL+3.0)
                        diff=max(0.0,diff)
                    diff=max(-40.0,min(24.0,diff))
                    tgtG=10**(diff/20.0)
            else:
                tgtG=min(g,1.0)
            upGap=20*math.log10(max(1e-6,tgtG))-20*math.log10(max(1e-6,g))
            upMs=upBaseMs*(1+2*math.exp(-abs(upGap)/3.0))
            upC=math.exp(-BLK/(FS*upMs/1000.0))
            cf=(warmCoeff if cutOnly else downCoeff) if tgtG<g else upC
            g=g*cf+tgtG*(1-cf)
            timeline.append((t,20*math.log10(max(1e-6,g)),signal))
    return timeline

for mode in ('Short','Integrated'):
    tl=run(mode)
    print("=== Rezim %s (cil -14, vstup -26 -> ocek. boost ~+12 dB) ===" % mode)
    marks=[3.0,6.8,7.5,8.0,12.0,14.0]   # konec 1. stopy, mezera, po mezere, ustaleno
    for m in marks:
        v=min(tl,key=lambda x:abs(x[0]-m))
        print("  t=%5.1fs  gain=%+6.2f dB  %s" % (v[0],v[1],"signal" if v[2] else "TICHO"))
    print()
