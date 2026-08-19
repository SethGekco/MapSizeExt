#!/usr/bin/env python3
"""Regenerate exact 0x40000 operand sites from a pinned PE for review."""
import argparse, struct
from pathlib import Path
p=argparse.ArgumentParser(); p.add_argument('exe'); p.add_argument('--output',required=True); a=p.parse_args()
data=Path(a.exe).read_bytes(); pe=struct.unpack_from('<I',data,0x3c)[0]; n=struct.unpack_from('<H',data,pe+6)[0]; osz=struct.unpack_from('<H',data,pe+20)[0]; opt=pe+24; base=struct.unpack_from('<I',data,opt+28)[0]; sh=opt+osz
secs=[]
for i in range(n):
 o=sh+40*i; secs.append((struct.unpack_from('<I',data,o+12)[0],struct.unpack_from('<I',data,o+8)[0],struct.unpack_from('<I',data,o+20)[0],struct.unpack_from('<I',data,o+16)[0]))
def read(va,n):
 r=va-base
 for sv,vs,raw,rs in secs:
  if sv<=r<sv+max(vs,rs): return data[raw+r-sv:raw+r-sv+n]
 raise ValueError(hex(va))
found={'CmpEax':[],'Push':[],'CmpReg':[]}; va=0x401000
while va<0x7e038d-6:
 b=read(va,6)
 if b[0] in (0x3d,0x68) and struct.unpack_from('<I',b,1)[0]==0x40000:
  found['CmpEax' if b[0]==0x3d else 'Push'].append(va); va+=5
 elif b[0]==0x81 and 0xf8<=b[1]<=0xff and struct.unpack_from('<I',b,2)[0]==0x40000:
  found['CmpReg'].append(va); va+=6
 else: va+=1
skip={0x565B73,0x568710,0x5687A7,0x568B58}
lines=['#pragma once',
       '// Generated from the pinned YR 1.001 executable. Do not runtime-scan .text:',
       '// these exact addresses are the reviewed production authority. Values are',
       '// derived from PlaneScale at runtime after full-profile gating.']
for name,values in found.items():
 values=[v for v in values if v not in skip]
 lines.append(f'static const DWORD kBounds{name}Sites[] = {{')
 for i in range(0,len(values),8): lines.append('    '+', '.join(f'0x{v:08X}' for v in values[i:i+8])+',')
 lines += ['};',f'static const int kBounds{name}Sites_n = {len(values)};']
Path(a.output).write_text('\n'.join(lines)+'\n')
