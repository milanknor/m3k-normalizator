# -*- coding: utf-8 -*-
# Pumpovani v Momentary rezimu: momentary klesa v tichych pasazich -> boost honi propad.
# Test: cap boostu dle integrated (stabilni celkova hlasitost stopy).
import math
from collections import deque
SR=48000.0; BLK=512; BLK_MS=BLK/SR*1000.0
MOM=int(SR*0.4)
def L2p(L): return 0.0 if L<=-190 else 10.0**((L+0.691)/10.0)
def p2L(p): return -70.0 if p<=1e-10 else -0.691+10.0*math.log10(p)

def signal():
    # 600 ms hlasite (-18), 400 ms tise (-30) -> momentary kolisa, short/int stabilni
    out=[]; loud=int(600/BLK_MS); quiet=int(400/BLK_MS)
    for k in range(40):
        out += [-18.0]*loud + [-30.0]*quiet
    return out

def run(target, speed_ms, cap_mode):
    mom_n=max(1,int(round(MOM/float(BLK))))
    momD=deque(maxlen=mom_n)
    intSum=0.0;intCnt=0; gain=1.0; lim=1.0
    downC=math.exp(-BLK/(SR*0.010)); upC=math.exp(-BLK/(SR*speed_ms/1000.0))
    ceiling=10.0**(-1.0/20.0)
    gmin=99;gmax=-99;limhits=0
    blocks=signal()
    for bi,Lblk in enumerate(blocks):
        p=L2p(Lblk); momD.append(p)
        active=p2L(p)>-70.0
        if active: intSum+=p*BLK; intCnt+=BLK
        mom=p2L(sum(momD)/len(momD)); intg=p2L(intSum/intCnt) if intCnt>0 else -70.0
        ref=mom  # Momentary rezim
        diff=target-ref
        if diff>0:
            if cap_mode=='mom': diff=min(diff,target-mom+3.0)
            elif cap_mode=='int' and intCnt>int(SR*0.4): diff=min(diff,target-intg+3.0)
            diff=max(0.0,diff)
        diff=max(-40,min(24,diff)); tg=10**(diff/20.0)
        coeff=downC if tg<gain else upC; gain=gain*coeff+tg*(1-coeff)
        gdb=20*math.log10(gain)
        outpk=10.0**((Lblk+3.0+gdb)/20.0)
        if outpk>ceiling: lim=min(lim,ceiling/outpk)
        else: lim=lim*0.9+0.1
        if bi>120:
            gmin=min(gmin,gdb);gmax=max(gmax,gdb)
            if lim<0.8: limhits+=1
    return gmin,gmax,limhits

print "Momentary, stopa -18/-30 stridave, target -16, Speed 10ms"
print "%-14s %7s %7s %8s" % ("cap","gMin","gMax","limHits")
for c in ('none','mom','int'):
    g=run(-16.0,10.0,c); print "%-14s %7.1f %7.1f %8d"%(c,g[0],g[1],g[2])
print
print "Kontrola: opravdu TICHA stopa -28, target -16 (boost ma dojet ~+12)"
def run2(cap_mode):
    mom_n=max(1,int(round(MOM/float(BLK)))); momD=deque(maxlen=mom_n)
    intSum=0.0;intCnt=0;gain=1.0
    downC=math.exp(-BLK/(SR*0.010)); upC=math.exp(-BLK/(SR*10/1000.0))
    for Lblk in [-28.0]*400:
        p=L2p(Lblk); momD.append(p); intSum+=p*BLK; intCnt+=BLK
        mom=p2L(sum(momD)/len(momD)); intg=p2L(intSum/intCnt)
        diff=-16.0-mom
        if diff>0:
            if cap_mode=='int' and intCnt>int(SR*0.4): diff=min(diff,-16.0-intg+3.0)
            diff=max(0,diff)
        diff=max(-40,min(24,diff)); tg=10**(diff/20.0)
        coeff=downC if tg<gain else upC; gain=gain*coeff+tg*(1-coeff)
    return 20*math.log10(gain)
print "  cap none: %.1f dB" % run2('none')
print "  cap int:  %.1f dB" % run2('int')
