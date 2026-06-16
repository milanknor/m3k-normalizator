# -*- coding: utf-8 -*-
# Test pumpovani v Custom rezimu s kratkym oknem + oprava (boost cap dle momentary).
import math
from collections import deque
SR=48000.0; BLK=512; BLK_MS=BLK/SR*1000.0
MOM=int(SR*0.4)
def L2p(L): return 0.0 if L<=-190 else 10.0**((L+0.691)/10.0)
def p2L(p): return -70.0 if p<=1e-10 else -0.691+10.0*math.log10(p)

def signal_gappy(loud,gap,nblocks):
    # loud track with a short quiet gap every ~250 ms
    out=[]
    period=int(250/BLK_MS); gaplen=max(1,int(40/BLK_MS))
    for i in range(nblocks):
        out.append(gap if (i%period)<gaplen else loud)
    return out

def run(custom_ms, target, speed_ms, boost_cap):
    cu_n=max(1,int(round(custom_ms/BLK_MS))); mom_n=max(1,int(round(MOM/float(BLK))))
    cuD=deque(maxlen=cu_n); momD=deque(maxlen=mom_n)
    gain=1.0; lim=1.0
    downC=math.exp(-BLK/(SR*0.010)); upC=math.exp(-BLK/(SR*speed_ms/1000.0))
    ceiling=10.0**(-1.0/20.0)
    blocks=signal_gappy(-14.0,-35.0,560)
    gmin=99;gmax=-99;limmin=1.0;limhits=0;outmax=-200
    for bi,Lblk in enumerate(blocks):
        p=L2p(Lblk); cuD.append(p); momD.append(p)
        active=p2L(p)>-70.0
        cst=p2L(sum(cuD)/len(cuD)); mom=p2L(sum(momD)/len(momD))
        targetGain=1.0
        if active:
            ref=cst
            diff=target-ref
            if diff>0:  # boost
                if boost_cap:                # cap boost by stable momentary
                    diff=min(diff, target-mom+3.0)
                    diff=max(0.0,diff)
            diff=max(-40.0,min(24.0,diff))
            targetGain=10.0**(diff/20.0)
        coeff=downC if targetGain<gain else upC
        gain=gain*coeff+targetGain*(1.0-coeff)
        gdb=20*math.log10(gain)
        # limiter (block approx): peak ~ Lblk+gain as dBFS-ish (sine peak = L+3)
        outpk=10.0**((Lblk+3.0+gdb)/20.0)
        if outpk>ceiling:
            need=ceiling/outpk
            lim=min(lim,need);
        else:
            lim=lim*0.95+1.0*0.05
        outdb=20*math.log10(min(outpk,ceiling*lim if lim<1 else outpk))
        if bi>40:  # skip initial settle
            gmin=min(gmin,gdb);gmax=max(gmax,gdb)
            limmin=min(limmin,lim)
            if lim<0.7: limhits+=1
    return gmin,gmax,limmin,limhits

for cms in (15,50):
  for sp in (10.0,50.0):
    print "Custom okno %dms, stopa -14 s mezerami -35, target -10, Speed %dms" % (cms,sp)
    print "%-18s %7s %7s %7s %8s" % ("varianta","gainMin","gainMax","limMin","limHits")
    g=run(cms,-10.0,sp,False); print "%-18s %7.1f %7.1f %7.2f %8d" % ("BEZ opravy",g[0],g[1],g[2],g[3])
    g=run(cms,-10.0,sp,True);  print "%-18s %7.1f %7.1f %7.2f %8d" % ("S boost-cap mom",g[0],g[1],g[2],g[3])
    print
print
print "Kontrola: TICHA stopa (boost ma fungovat) -28, target -16, okno 50ms"
def run2(boost_cap):
    cu_n=max(1,int(round(50/BLK_MS))); mom_n=max(1,int(round(MOM/float(BLK))))
    cuD=deque(maxlen=cu_n); momD=deque(maxlen=mom_n); gain=1.0
    downC=math.exp(-BLK/(SR*0.010)); upC=math.exp(-BLK/(SR*300/1000.0))
    blocks=[-28.0]*560
    for Lblk in blocks:
        p=L2p(Lblk); cuD.append(p); momD.append(p)
        cst=p2L(sum(cuD)/len(cuD)); mom=p2L(sum(momD)/len(momD))
        diff=-16.0-cst
        if diff>0 and boost_cap: diff=min(diff,-16.0-mom+3.0); diff=max(0,diff)
        diff=max(-40,min(24,diff)); tg=10**(diff/20.0)
        coeff=downC if tg<gain else upC; gain=gain*coeff+tg*(1-coeff)
    return 20*math.log10(gain)
print "  finalni gain BEZ opravy:   %.1f dB (ma byt ~+12)" % run2(False)
print "  finalni gain S boost-cap:  %.1f dB (ma byt ~+12)" % run2(True)
