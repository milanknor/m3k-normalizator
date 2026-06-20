#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Diagnostika true-peak limiteru: replikuje gain-follower logiku z PluginProcessor.cpp
a meri zkresleni (THD) + tvrde orezy na hlasitem tonu nad stropem.
Porovna STAVAJICI limiter vs OPRAVENY (sliding-min lookahead, peak-hold).
"""
import math

R   = 192000.0          # "oversampled" rychlost
C   = 10**(-0.3/20.0)   # strop -0.3 dBFS
L   = int(0.0015*R)     # lookahead ~1.5 ms
F0  = 100.0             # nizky ton (kytara), bohaty na harmonicke pri zkresleni
AMP = 1.6               # peak 1.6 > strop -> limiter musi brat ~ -4.4 dB
SEC = 0.5

def gen():
    n = int(SEC*R)
    return [AMP*math.sin(2*math.pi*F0*i/R) for i in range(n)]

# ---- STAVAJICI limiter (verbatim logika) ----
def limiter_current(x):
    atk = 1.0 - math.exp(-3.0/L)
    rel = 1.0 - math.exp(-1.0/(R*0.15))
    g = 1.0
    delay = [0.0]*L
    wp = 0
    clips = 0
    out = []
    for xn in x:
        peak = abs(xn)
        gReq = (C/peak) if peak > C else 1.0
        g += (gReq - g)*(atk if gReq < g else rel)
        d = delay[wp]; delay[wp] = xn
        wp = (wp+1) % L
        y = d*g
        if abs(y) > C + 1e-9: clips += 1
        y = max(-1.0, min(1.0, y))      # jlimit
        out.append(y)
    return out, clips

# ---- OPRAVENY limiter: sliding-window-min (peak-hold) pres lookahead + vyhlazeni ----
def limiter_fixed(x):
    from collections import deque
    atk = 1.0 - math.exp(-4.0/L)          # plynuly utok, dosahne cile behem lookaheadu
    rel = 1.0 - math.exp(-1.0/(R*0.05))   # ~50 ms release
    atk2 = 1.0 - math.exp(-6.0/L)          # 2. stupen vyhlazeni (zaobli hrany obalky)
    greq = [ (C/abs(v)) if abs(v) > C else 1.0 for v in x ]
    win = deque()      # monotonni fronta (index, gReq), rostouci dle gReq -> front = min
    g1 = 1.0; g2 = 1.0
    delay = [0.0]*L
    wp = 0
    clips = 0
    out = []
    n = len(x)
    for i in range(n):
        gj = greq[i]                       # pridej aktualni vzorek do okna [i-L+1, i]
        while win and win[-1][1] >= gj: win.pop()
        win.append((i, gj))
        while win and win[0][0] <= i - L: win.popleft()
        target = win[0][1]                 # min gReq pres delay-line (budouci vystupy)
        g1 += (target - g1)*(atk  if target < g1 else rel)
        g2 += (g1 - g2)*(atk2 if g1 < g2 else rel)   # kaskadni vyhlazeni
        d = delay[wp]; delay[wp] = x[i]    # output = x[i-L]
        wp = (wp+1) % L
        y = d*g2
        if abs(y) > C + 1e-9: clips += 1
        y = max(-C, min(C, y))             # brickwall presne na stropu
        out.append(y)
    return out, clips

def goertzel(x, f):
    w = 2*math.pi*f/R; cw = math.cos(w); coeff = 2*cw
    s0=s1=s2=0.0
    for xn in x: s0 = xn + coeff*s1 - s2; s2=s1; s1=s0
    return math.sqrt(max(0.0, s1*s1 + s2*s2 - coeff*s1*s2))

def thd_pct(x):
    skip = int(0.05*R)
    seg = x[skip:]
    f1 = goertzel(seg, F0)
    h = 0.0
    for k in range(2, 16):
        h += goertzel(seg, k*F0)**2
    return 100.0*math.sqrt(h)/f1 if f1>0 else 0.0

sig = gen()
oc, ocl = limiter_current(sig)
of, ofl = limiter_fixed(sig)
print("Vstup: %.1f Hz, peak %.2f (strop %.3f), pozadovany rez ~ %.1f dB"
      % (F0, AMP, C, 20*math.log10(C/AMP)))
print()
print("STAVAJICI limiter:  THD = %6.3f %%   tvrde orezy = %d" % (thd_pct(oc), ocl))
print("OPRAVENY  limiter:  THD = %6.3f %%   tvrde orezy = %d" % (thd_pct(of), ofl))
print("max |out| stavajici=%.4f  opraveny=%.4f  strop=%.4f"
      % (max(abs(v) for v in oc), max(abs(v) for v in of), C))
