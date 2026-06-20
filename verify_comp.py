#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Overeni mezistupnoveho kompresoru: feed-forward, soft-knee, loudness-neutralni
auto-makeup (podle bezici prumerne gain reduction). Overuje:
  1) staticka krivka (vstup->vystup) = spravny ratio,
  2) loudness-neutralita = prumerna uroven se NEZmeni (cil zachovan),
  3) redukce dynamiky (kolisani out < kolisani in).
"""
import math
FS=48000.0
def g2db(x): return -120.0 if x<=1e-12 else 20.0*math.log10(x)
def db2g(d): return 10.0**(d/20.0)

def compressor(x, thr, ratio, knee=6.0, atk_ms=10.0, rel_ms=150.0):
    atk=math.exp(-1.0/(FS*atk_ms/1000.0))
    rel=math.exp(-1.0/(FS*rel_ms/1000.0))
    env=-120.0          # detektor v dB
    avgGr=0.0           # bezici prumer GR (pro loudness-neutralni makeup)
    avgC=math.exp(-1.0/(FS*0.3))   # ~300 ms prumer
    out=[]
    for s in x:
        lvl=g2db(abs(s))
        c = atk if lvl>env else rel
        env = env*c + lvl*(1-c)
        # soft-knee staticka krivka -> pozadovana GR (dB, <=0)
        over=env-thr
        if over<=-knee/2: gr=0.0
        elif over>=knee/2: gr=over*(1.0/ratio-1.0)
        else:
            # kvadraticke koleno
            gr=(1.0/ratio-1.0)*((over+knee/2)**2)/(2*knee)
        avgGr = avgGr*avgC + gr*(1-avgC)
        makeup = -avgGr            # loudness-neutralni
        out.append(s*db2g(gr+makeup))
    return out

def steady(level_db, secs=1.0, f=300.0):
    n=int(secs*FS); a=db2g(level_db)*math.sqrt(2)
    return [a*math.sin(2*math.pi*f*i/FS) for i in range(n)]

def rms_db(x):
    s=sum(v*v for v in x)/len(x); return 10*math.log10(s) if s>0 else -120

print("=== Staticka krivka (thr=-18, ratio=3) — vstup vs vystup RMS ===")
for L in [-30,-24,-18,-12,-6,-3]:
    o=compressor(steady(L),-18.0,3.0)
    print("  in %+5.1f dB -> out %+6.2f dB" % (L, rms_db(o[int(0.5*FS):])))
print("  (nad prahem -18 ma byt sklon ~1/3; pod prahem ~1:1)")

print("\n=== Loudness-neutralita: dynamicky signal (strida -10 a -26 dB) ===")
sig=[]
for _ in range(4): sig+=steady(-10,0.5); sig+=steady(-26,0.5)
o=compressor(sig,-18.0,3.0)
seg=int(0.5*FS)
ins=[rms_db(sig[k*seg:(k+1)*seg]) for k in range(8)]
outs=[rms_db(o[k*seg:(k+1)*seg]) for k in range(8)]
print("  IN  segmenty dB:", " ".join("%+5.1f"%v for v in ins))
print("  OUT segmenty dB:", " ".join("%+5.1f"%v for v in outs))
print("  rozkmit IN = %.1f dB,  OUT = %.1f dB (ma se zmensit)" %
      (max(ins)-min(ins), max(outs)-min(outs)))
print("  prumer IN = %+.2f,  OUT = %+.2f dB (ma byt ~stejny = loudness-neutralni)" %
      (sum(ins)/len(ins), sum(outs)/len(outs)))
