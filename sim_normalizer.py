# -*- coding: utf-8 -*-
# Simulace logiky normalizace M3K Normalizator (block-level) pro hledani naporu/najizdeni.
import math, random
from collections import deque

SR = 48000.0
BLK = 512
BLK_MS = BLK / SR * 1000.0
MOM = int(SR*0.4); SHORT = int(SR*3.0); MOMS = MOM

def L2p(L): return 0.0 if L <= -190 else 10.0**((L+0.691)/10.0)
def p2L(p): return -70.0 if p <= 1e-10 else -0.691 + 10.0*math.log10(p)

MODES = ["Mom","Short","Int","Custom","MomC","ShortC","IntC","CustomC"]

def build_signal(L1, L2, play_ms, sil_ms):
    def seg(L, ms, fade):
        n=int(ms/BLK_MS); out=[]
        for i in range(n):
            if L<=-190: out.append(-200.0)
            else:
                v=L+random.uniform(-1.5,1.5)
                if fade and i<3: v=L-30.0+i*10.0
                out.append(v)
        return out
    a=seg(L1,play_ms,True); s=seg(-200,sil_ms,False); b=seg(L2,play_ms,True)
    return a+s+b, len(a)+len(s)

# OPT: behavior options to compare
def simulate(mode, target, L1, L2, speed_ms, custom_ms, opt, seed=1):
    random.seed(seed)
    isInt = mode in (2,6)
    cu_n = max(1, int(round(custom_ms/BLK_MS)))
    mom_n = max(1, int(round(MOM/float(BLK))))
    sh_n  = max(1, int(round(SHORT/float(BLK))))
    momD=deque(maxlen=mom_n); shD=deque(maxlen=sh_n); cuD=deque(maxlen=cu_n)
    intSum=0.0; intCnt=0; activeSamples=0; gain=1.0
    downC = math.exp(-float(BLK)/(SR*opt['down_ms']/1000.0))
    warmDownC = math.exp(-float(BLK)/(SR*opt['warm_down_ms']/1000.0))
    upC   = math.exp(-float(BLK)/(SR*speed_ms/1000.0))
    blocks,resume_idx = build_signal(L1,L2,3000,1000)
    out=[]; gains=[]
    for Lblk in blocks:
        p=L2p(Lblk)
        momD.append(p); shD.append(p); cuD.append(p)
        blockLoud=p2L(p)
        active = blockLoud > -70.0
        activeSamples = activeSamples+BLK if active else 0
        if active: intSum+=p*BLK; intCnt+=BLK
        elif opt.get('reset_int_sil'): intSum=0.0; intCnt=0
        mom=p2L(sum(momD)/len(momD)); sht=p2L(sum(shD)/len(shD)); cst=p2L(sum(cuD)/len(cuD))
        intg=p2L(intSum/intCnt) if intCnt>0 else -70.0

        windowNeeded=MOM
        if mode in (1,5): windowNeeded=SHORT
        elif mode in (2,6): windowNeeded=int(SR*0.4)
        elif mode in (3,7): windowNeeded=cu_n*BLK
        int_imm = isInt and opt.get('int_immediate',True)
        windowReady = int_imm or (activeSamples>=windowNeeded)

        cutOnly=False
        if active:
            ref=-70.0; intMin=int(SR*0.4)
            if windowReady:
                if mode in (0,4): ref=mom
                elif mode in (1,5): ref=sht
                elif mode in (2,6): ref=intg if intCnt>intMin else -70.0
                else: ref=cst
            else:
                ref=blockLoud; cutOnly=True
            targetGain=1.0
            if ref>-69.0:
                diff=target-ref
                allowBoost=(not cutOnly) and (int_imm or activeSamples>=MOMS)
                if not allowBoost: diff=min(0.0,diff)
                diff=max(-40.0,min(24.0,diff))
                targetGain=10.0**(diff/20.0)
        else:
            # silence behaviour
            if opt['hold_cut']:
                targetGain=min(gain,1.0)   # keep cuts, relax boosts to unity
            else:
                targetGain=1.0
        # choose coefficient
        if targetGain<gain:
            coeff = warmDownC if (cutOnly and opt['warm_down_ms']<opt['down_ms']) else downC
        else:
            coeff = upC
        gain = gain*coeff + targetGain*(1.0-coeff)
        gdb=20.0*math.log10(gain) if gain>1e-9 else -180.0
        out.append(Lblk+gdb if Lblk>-190 else -200.0); gains.append(gdb)
    return blocks,out,gains,resume_idx

def analyze(mode,target,L1,L2,speed_ms,custom_ms,opt):
    blocks,out,gains,ri=simulate(mode,target,L1,L2,speed_ms,custom_ms,opt)
    w=int(500/BLK_MS)
    seg=[x for x in out[ri:ri+w] if x>-190]
    peak=max(seg) if seg else -200
    over=peak-target
    return over

# scenarios: (name, target, L1, L2, speed)
SCEN=[
 ("rez maly  -15->-16",      -16.0,-15.0,-15.0,300.0),
 ("rez velky -8 ->-20",      -20.0, -8.0, -8.0,300.0),
 ("rez pomaly -10->-23 s2000",-23.0,-10.0,-10.0,2000.0),
 ("rez rychly -8->-20 s10",  -20.0, -8.0, -8.0,10.0),
 ("boost -28->-16",          -16.0,-28.0,-28.0,300.0),
 ("cross tichá->HLASITA",    -20.0,-28.0, -8.0,300.0),
 ("cross HLASITA->ticha",    -16.0, -8.0,-28.0,300.0),
 ("cross stejny target boost",-12.0,-30.0,-6.0,300.0),
]
OPTS={
 'V1 soucasny':           dict(hold_cut=False,down_ms=30.0,warm_down_ms=30.0,int_immediate=True),
 'V3 hold+down10':        dict(hold_cut=True, down_ms=10.0,warm_down_ms=10.0,int_immediate=True),
 'V5 hold+d10+intReset':  dict(hold_cut=True, down_ms=10.0,warm_down_ms=5.0,int_immediate=False,reset_int_sil=True),
}
for oname in ['V1 soucasny','V3 hold+down10','V5 hold+d10+intReset']:
    opt=OPTS[oname]
    print "######## %s ########" % oname
    print "%-26s | %s" % ("scenar","max over_dB pres rezimy (max ze vsech 8)")
    for (sname,tg,L1,L2,sp) in SCEN:
        worst=-99; worstmode=0
        for m in range(8):
            cms=50 if m in (3,7) else 1000
            o=analyze(m,tg,L1,L2,sp,cms,opt)
            if o>worst: worst=o; worstmode=m
        flag="  <-- NAPOR" if worst>3.0 else ""
        print "%-26s | %6.1f (%s)%s" % (sname,worst,MODES[worstmode],flag)
    print
