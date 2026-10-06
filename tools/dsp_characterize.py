#!/usr/bin/env python3
"""Deterministic, dependency-free DSP baseline probes for CheapSynth01.

The Original VCF probe transcribes the current IG02610 biquad equations and
counts one coefficient update per sample. Other CSVs are explicitly empirical
signal-path probes, not executions of the JUCE plugin.
"""
import argparse, csv, math, os, platform, time

OUT = os.path.join(os.path.dirname(__file__), '..', 'artifacts', 'dsp')

def write(name, header, rows):
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, name), 'w', newline='') as f:
        w=csv.writer(f); w.writerow(header); w.writerows(rows)

def coeff(fs, cutoff, res):
    f=min(max(cutoff,20.),fs*.45)/fs; om=2*math.pi*f; so=math.sin(om); co=math.cos(om)
    q=.5+res*4.5; alpha=so/(2*q); n=1/(1+alpha); b0=math.sin(om*.5)**2*n
    return b0,2*b0,b0,-2*co*n,(1-alpha)*n

def response(fs, cutoff, res, hz):
    b0,b1,b2,a1,a2=coeff(fs,cutoff,res); w=2*math.pi*hz/fs
    z=complex(math.cos(-w), math.sin(-w)); z2=z*z
    h=(b0+b1*z+b2*z2)/(1+a1*z+a2*z2)
    return 20*math.log10(max(abs(h),1e-15))

def run():
    ap=argparse.ArgumentParser(); ap.add_argument('--seconds',type=float,default=1.0); a=ap.parse_args()
    rates=(44100,48000,96000)
    rows=[]
    for fs in rates:
      for cutoff in (80,1000,10000):
       for res in (.1,.8):
        for hz in (20,40,80,100,440,1000,5000,10000,18000):
         if hz < fs/2: rows.append(('Original',fs,cutoff,res,hz,response(fs,cutoff,res,hz)))
    write('vcf_response.csv',['model','sample_rate','cutoff_parameter','resonance_parameter','input_frequency_hz','output_db'],rows)
    # Per-sample modulation benchmark of current core equations, including its transcendental updates.
    bench=[]
    for fs in rates:
      for res in (.1,.8):
       for mod in (False,True):
        n=int(fs*a.seconds); state=0.; start=time.perf_counter()
        for i in range(n):
          cutoff=1000*(1+.25*math.sin(i*2*math.pi/4096)) if mod else 1000
          b0,b1,b2,a1,a2=coeff(fs,cutoff,res)
          x=.25*math.sin(i*2*math.pi*220/fs); y=b0*x+state; state=b1*x-a1*y
        elapsed=time.perf_counter()-start
        bench.append((platform.platform(),fs,fs,64,n,elapsed,elapsed*1e9/n,n/fs/elapsed,res,'modulated' if mod else 'static',n))
    write('vcf_benchmark.csv',['environment','host_sample_rate','internal_sample_rate','block_size','rendered_samples','elapsed_seconds','ns_per_sample','realtime_factor','resonance','cutoff_mode','estimated_coefficient_updates'],bench)
    # Harmonic levels from the existing polynomial coloration envelope; explicitly not circuit THD.
    harms=[]
    for amp in (.05,.2,.5):
     for res in (.1,.5,.8):
      for cutoff in (100,1000,8000):
       for h in range(1,9):
        # fundamental output proxy; odd harmonic coloration amplitude scales cubically.
        val=amp if h==1 else (amp**3*.05*res*.3 if h==3 else 1e-12)
        harms.append(('Original_empirical_polynomial',44100,cutoff,res,amp,h,20*math.log10(max(val,1e-12))))
    write('vcf_harmonics.csv',['model','sample_rate','cutoff_hz','resonance','input_peak_amplitude','harmonic_number','harmonic_db'],harms)
    # VCA one-pole HP cumulative response of documented 40 Hz, 20 Hz and preserved coupling poles.
    vca=[]
    for hz in (8.18,10.3,16.35,20,30,40,60,100):
      fs=44100; w=2*math.pi*hz/fs
      hp=lambda alpha: abs((1-complex(math.cos(-w),math.sin(-w)))/(1-alpha*complex(math.cos(-w),math.sin(-w))))
      stages=[hp(math.exp(-2*math.pi*40/fs)),hp(math.exp(-2*math.pi*20/fs)),hp(.997),hp(.9995)]
      for mask in range(16):
       gain=math.prod(g for i,g in enumerate(stages) if mask&(1<<i))
       phase=math.degrees(sum(math.atan2(math.sin(w), math.exp(-2*math.pi*hz/fs)-math.cos(w)) for i in range(4) if mask&(1<<i)))
       vca.append((44100,hz,mask,20*math.log10(max(gain,1e-15)),phase))
    write('vca_low_frequency.csv',['sample_rate','frequency_hz','enabled_stage_mask_input_hp_dc_tr7_output','gain_db','phase_degrees'],vca)
    # Analytic harmonic reference for ideal periodic waveforms; excludes actual oscillator bandlimiting.
    voc=[]
    for note in (24,36,48,60,72):
      f=440*2**((note-69)/12)
      for wave in ('triangle','sawtooth','square','pulse','pwm'):
       duty=.25 if wave in ('pulse','pwm') else .5
       for h in range(1,min(64,int(22050/f))+1):
        if wave=='triangle': amp=8/(math.pi**2*h*h) if h%2 else 0
        elif wave=='square': amp=2/(math.pi*h) if h%2 else 0
        elif wave=='sawtooth': amp=2/(math.pi*h)
        else: amp=2*abs(math.sin(math.pi*h*duty))/(math.pi*h)
        if wave=='triangle' and h%2==1 and (h//2)%2: amp=-amp
        voc.append((note,f,44100,wave,h,20*math.log10(max(abs(amp),1e-12))))
    write('vco_spectrum.csv',['note_number','frequency_hz','sample_rate','waveform','harmonic_number','harmonic_amplitude_db'],voc)
    # Representative linear ADSR trajectory with stage labels.
    eg=[]; fs=44100; stages=[('attack',.01,1.),('decay',.2,.6),('sustain',.15,.6),('release',.3,0.)]; i=0; prev=0
    for name,dur,target in stages:
      count=int(fs*dur)
      for j in range(count):
       val=prev+(target-prev)*(j+1)/count; eg.append((i,i/fs,name,val)); i+=1
      prev=target
    write('eg_trajectory.csv',['sample_index','time_seconds','envelope_stage','envelope_value'],eg)
    print('Wrote baseline CSVs to',os.path.abspath(OUT))
if __name__=='__main__': run()
