#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Overeni LUFS mereni M3K Normalizatoru proti ITU-R BS.1770-4 / EBU R128.

Replikuje PRESNE C++ kod (computeKWeightingCoefficients + processBiquad +
integrated LUFS = -0.691 + 10*log10(sum_kanalu(mean_square_K-vazene))).
Pokud je k dispozici pyloudnorm, provede i krizovou kontrolu.
"""
import math, cmath

FS = 48000.0

# ---- K-weighting koeficienty (verbatim z PluginProcessor.cpp) ----
def k_coeffs(fs):
    pi = math.pi
    # Stage 1: high-shelf
    Vh, Vb, Q, fc = 1.58486, 1.25872, 0.71054, 1681.974450955533
    K = math.tan(pi*fc/fs); Ks = K*K; d = 1 + K/Q + Ks
    hsB = [(Vh+Vb*K/Q+Ks)/d, 2*(Ks-Vh)/d, (Vh-Vb*K/Q+Ks)/d]
    hsA = [1.0,              2*(Ks-1)/d,   (1-K/Q+Ks)/d]
    # Stage 2: RLB high-pass
    Q2, fc2 = 0.5003270373238773, 38.13547087602444
    K = math.tan(pi*fc2/fs); Ks = K*K; d = 1 + K/Q2 + Ks
    rlbB = [1/d, -2/d, 1/d]
    rlbA = [1.0, 2*(Ks-1)/d, (1-K/Q2+Ks)/d]
    return hsB, hsA, rlbB, rlbA

HSB, HSA, RLBB, RLBA = k_coeffs(FS)

def biquad_mag_db(b, a, f, fs):
    w = 2*math.pi*f/fs
    z1 = cmath.exp(-1j*w); z2 = z1*z1
    num = b[0] + b[1]*z1 + b[2]*z2
    den = a[0] + a[1]*z1 + a[2]*z2
    return 20*math.log10(abs(num/den))

def k_weight_db(f):
    return biquad_mag_db(HSB, HSA, f, FS) + biquad_mag_db(RLBB, RLBA, f, FS)

# ---- processBiquad (transposed direct form II, verbatim) ----
def filt(x, b, a):
    z1 = z2 = 0.0
    out = [0.0]*len(x)
    for i, xn in enumerate(x):
        y = b[0]*xn + z1
        z1 = b[1]*xn - a[1]*y + z2
        z2 = b[2]*xn - a[2]*y
        out[i] = y
    return out

def integrated_lufs(channels):
    """channels: list of sample-lists (same length). EBU R128 channel-summed."""
    total = 0.0
    for ch in channels:
        k = filt(filt(ch, HSB, HSA), RLBB, RLBA)
        # zahod prvnich 0.2 s kvuli ustaleni filtru
        n0 = int(0.2*FS)
        seg = k[n0:]
        ms = sum(v*v for v in seg)/len(seg)
        total += ms
    return -0.691 + 10*math.log10(total)

def sine(freq, rms_dbfs, secs=2.0):
    amp = (10**(rms_dbfs/20.0))*math.sqrt(2.0)  # rms -> peak amplitude
    n = int(secs*FS)
    return [amp*math.sin(2*math.pi*freq*i/FS) for i in range(n)]

print("=== K-weighting koeficienty (FS=48000) ===")
print("HS  B:", HSB); print("HS  A:", HSA)
print("RLB B:", RLBB); print("RLB A:", RLBA)

print("\n=== K-weighting magnituda (dB) vs publikovana BS.1770 krivka ===")
for f in [20, 60, 100, 500, 997, 1000, 2000, 6000, 12000, 20000]:
    print("  %6d Hz : %+6.2f dB" % (f, k_weight_db(f)))
print("  (HF plato high-shelf ma byt ~ +3.99 dB; RLB rolloff dole)")

print("\n=== Integrated LUFS – stereo 1 kHz sine ===")
for lvl in [-23.0, -20.0, -14.0, -6.0]:
    L = sine(1000.0, lvl); R = sine(1000.0, lvl)
    print("  RMS %+5.1f dBFS/kanal -> %+7.2f LUFS" % (lvl, integrated_lufs([L, R])))

print("\n=== Kontroly konzistence ===")
base = integrated_lufs([sine(1000.0,-23.0), sine(1000.0,-23.0)])
up6  = integrated_lufs([sine(1000.0,-17.0), sine(1000.0,-17.0)])
print("  Linearita: +6 dB vstup -> zmena %.3f LU (ma byt 6.00)" % (up6-base))
mono = integrated_lufs([sine(1000.0,-23.0)])
stereo = integrated_lufs([sine(1000.0,-23.0), sine(1000.0,-23.0)])
print("  Soucet kanalu: stereo-mono = %.3f LU (ma byt 3.01)" % (stereo-mono))

# najdi presnou RMS uroven pro -23.0 LUFS
def lufs_of_stereo_sine(rms):
    return integrated_lufs([sine(1000.0,rms), sine(1000.0,rms)])
lo, hi = -40.0, 0.0
for _ in range(40):
    mid = (lo+hi)/2
    if lufs_of_stereo_sine(mid) < -23.0: lo = mid
    else: hi = mid
print("  Stereo 1 kHz sine pro presne -23.0 LUFS: RMS = %.2f dBFS/kanal" % ((lo+hi)/2))

# ---- volitelny krizovy test proti pyloudnorm ----
try:
    import numpy as np, pyloudnorm as pyln
    print("\n=== Krizova kontrola: pyloudnorm (gold standard) ===")
    meter = pyln.Meter(int(FS))
    for lvl in [-23.0, -14.0]:
        L = np.array(sine(1000.0, lvl)); R = np.array(sine(1000.0, lvl))
        data = np.stack([L, R], axis=1)
        ref = meter.integrated_loudness(data)
        ours = integrated_lufs([list(L), list(R)])
        print("  %+5.1f dBFS: nase=%.2f  pyloudnorm=%.2f  rozdil=%.3f LU"
              % (lvl, ours, ref, ours-ref))
except Exception as e:
    print("\n(pyloudnorm nedostupny – krizovy test preskocen: %s)" % e)
