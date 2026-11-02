#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build immutable RSX programs with the PSL1GHT Cg compiler."""
import os, pathlib, subprocess, sys, tempfile
VP = r"""
void main(float4 pos : POSITION, float4 color : COLOR0, float2 uv : TEXCOORD0,
          uniform float4 page, uniform float4 window, uniform float4 effects,
          uniform float4 extra,
          out float4 oPos : POSITION, out float4 oColor : COLOR0,
          out float2 oUV : TEXCOORD0, out float4 oPage : TEXCOORD1,
          out float4 oWindow : TEXCOORD2, out float4 oEffects : TEXCOORD3,
          out float4 oExtra : TEXCOORD4, out float2 oPixel : TEXCOORD5) {
  oPos=pos; oColor=color; oUV=uv; oPage=page; oWindow=window;
  oEffects=effects; oExtra=extra; oPixel=(pos.xy+1)*float2(512,256);
}
"""
COMMON = r"""
float modn(float v, float n) { return v-floor(v/n)*n; }
float2 wrap(float2 xy) { return float2(modn(xy.x,1024),modn(xy.y+floor(xy.x/1024),512)); }
float4 sampleVRAM(sampler2D tex, float2 xy) {
  return tex2D(tex,(wrap(xy)+0.5)*float2(1.0/1024,1.0/512));
}
float word(float4 c) {
  float3 rgb=floor(floor(c.rgb*255+0.5)/8);
  return dot(rgb,float3(1,32,1024))+step(0.5,c.a)*32768;
}
float band5(float a, float b) {
  float r=0;
  r+=modn(a,2)*modn(b,2);
  r+=modn(floor(a/2),2)*modn(floor(b/2),2)*2;
  r+=modn(floor(a/4),2)*modn(floor(b/4),2)*4;
  r+=modn(floor(a/8),2)*modn(floor(b/8),2)*8;
  r+=modn(floor(a/16),2)*modn(floor(b/16),2)*16;
  return r;
}
// Keep integer texels on their side of floating-point interpolation error.
float windowCoord(float v,float mask,float offset) {
  v=modn(floor(v+0.0001),256);
  return v-band5(floor(v/8),mask)*8+offset;
}
float dither(float2 pos) {
  float x=modn(floor(pos.x),4), y=modn(floor(pos.y),4);
  float4 row=y<1 ? float4(0,7,1,6) : y<2 ? float4(5,2,4,3) : y<3 ? float4(6,1,7,0) : float4(3,4,2,5);
  return x<1 ? row.x : x<2 ? row.y : x<3 ? row.z : row.w;
}
"""
def fragment(mode):
    fetch = {
      0: "float4 texel=float4(1,1,1,0);",
      1: """float wx=page.x+floor(tc.x/4);
        float bits=word(sampleVRAM(tex,float2(wx,page.y+tc.y)));
        // Palette boundaries require exact binary scale factors.
        float phase=modn(tc.x,4);
        float shift=phase<0.5 ? 1 : phase<1.5 ? 0.0625 : phase<2.5 ? 0.00390625 : 0.000244140625;
        float index=modn(floor(bits*shift),16);
        float4 texel=sampleVRAM(tex,float2(page.z+index,page.w));""",
      2: """float wx=page.x+floor(tc.x/2);
        float bits=word(sampleVRAM(tex,float2(wx,page.y+tc.y)));
        float shift=modn(tc.x,2)<0.5 ? 1 : 0.00390625;
        float index=modn(floor(bits*shift),256);
        float4 texel=sampleVRAM(tex,float2(page.z+index,page.w));""",
      3: "float4 texel=sampleVRAM(tex,page.xy+tc);",
    }[mode]
    return COMMON + r"""
float4 main(float4 color:COLOR0, float2 uv:TEXCOORD0, float4 page:TEXCOORD1,
            float4 window:TEXCOORD2, float4 effects:TEXCOORD3,
            float4 extra:TEXCOORD4, float2 pos:TEXCOORD5,
            uniform sampler2D tex:TEXUNIT0) : COLOR {
  float2 tc=float2(windowCoord(uv.x,window.x,window.z),windowCoord(uv.y,window.y,window.w));
""" + fetch + ("\n  if(word(texel)<0.5) discard;\n" if mode else "\n") + r"""
  float4 dst=sampleVRAM(tex,floor(pos));
  if(effects.z>0.5 && dst.a>0.5) discard;
  float3 src=floor(color.rgb*255+0.5);
""" + (r"""
  src=extra.x>0.5 ? floor(floor(texel.rgb*255+0.5)/8)*8 : min(floor(floor(texel.rgb*255+0.5)/8)*src/16,255.0);
""" if mode else "") + r"""
  src=clamp(floor((src+effects.w*dither(pos))/8),0,31);
  float3 back=floor(floor(dst.rgb*255+0.5)/8);
""" + ("  float blend=texel.a>0.5 ? effects.x : 0;\n" if mode else "  float blend=effects.x;\n") + r"""
  if(blend>0.5) {
    if(blend<1.5) src=floor(src/2)+floor(back/2);
    else if(blend<2.5) src=min(src+back,31.0);
    else if(blend<3.5) src=max(back-src,0.0);
    else src=min(back+floor(src/4),31.0);
  }
  return float4(src*(8.0/255),max(effects.y,texel.a));
}
"""
def opaque_fragment(mode, use_dither):
    source=fragment(mode)
    source=source.replace('float2 tc=float2(windowCoord(uv.x,window.x,window.z),windowCoord(uv.y,window.y,window.w));',
                          'float2 v=floor(uv+0.0001); float2 tc=v-floor(v*extra.yz)*window.xy+window.zw;')
    source=source.replace('  float4 dst=sampleVRAM(tex,floor(pos));','').replace('  if(effects.z>0.5 && dst.a>0.5) discard;','')
    if use_dither: source=source.replace('effects.w*dither(pos)','dither(pos)')
    else: source=source.replace('effects.w*dither(pos)','0')
    first=source.index('  float3 back=')
    last=source.index('  return float4(src*',first)
    return source[:first]+source[last:]

def native_fragment(mode):
    source=fragment(mode)
    source=source.replace('  float4 dst=sampleVRAM(tex,floor(pos));','').replace('  if(effects.z>0.5 && dst.a>0.5) discard;','')
    source=source[:source.index('  float3 back=')]
    if mode:
        source += "  if(extra.w>0.5 && extra.w<1.5 && texel.a>0.5) discard;\n"
        source += "  if(extra.w>1.5 && texel.a<0.5) discard;\n"
        source += "  if(effects.x>3.5 && texel.a>0.5) src=floor(src/4);\n"
    else:
        source += "  if(effects.x>3.5) src=floor(src/4);\n"
    return source + "  return float4(src*(8.0/255),max(effects.y,texel.a));\n}\n"

DISPLAY15 = COMMON + r"""
float4 main(float2 uv:TEXCOORD0,uniform sampler2D tex:TEXUNIT0):COLOR {
  float4 c=sampleVRAM(tex,floor(uv+0.0001));
  return float4(floor(floor(c.rgb*255+0.5)/8)/31,1);
}
"""

COPY = COMMON + r"""
float4 main(float2 uv:TEXCOORD0, float4 effects:TEXCOORD3,
            float2 pos:TEXCOORD5, uniform sampler2D tex:TEXUNIT0):COLOR {
  if(effects.z>0.5 && sampleVRAM(tex,floor(pos)).a>0.5) discard;
  float4 c=sampleVRAM(tex,floor(uv+0.0001));
  c.a=max(c.a,effects.y);
  return c;
}
"""
DISPLAY24 = COMMON + r"""
// Keep RGB output together: cgcomp maps oCol onto temporary register R0.
float3 mod3(float3 v,float n) {return v-floor(v/n)*n;}
float3 bytesAt(sampler2D tex,float3 n) {
 n=mod3(n,1048576);
 float3 w=floor(n*0.5);
 float3 x=mod3(w,1024),y=floor(w/1024);
 float3 bits=float3(word(sampleVRAM(tex,float2(x.x,y.x))),word(sampleVRAM(tex,float2(x.y,y.y))),word(sampleVRAM(tex,float2(x.z,y.z))));
 float3 shift=1-step(0.5,mod3(n,2))*0.99609375;
 return mod3(floor(bits*shift),256)*(1.0/255);
}
float4 main(float2 uv:TEXCOORD0,float4 extra:TEXCOORD4,uniform sampler2D tex:TEXUNIT0):COLOR {
 float2 pixel=floor(uv+0.0001);
 float x=extra.x+pixel.x*3,y=pixel.y*2048;
 return float4(bytesAt(tex,y+mod3(x+float3(0,1,2),2048)),1);
}

"""
UPLOAD = r"""
float4 main(float2 uv:TEXCOORD0, float4 page:TEXCOORD1,
            uniform sampler2D tex:TEXUNIT0):COLOR {
  return tex2D(tex,(floor(uv+0.0001)+0.5)*page.xy);
}
"""
def main():
    compiler, target = sys.argv[1:]
    output=['/* Generated by shaders.py. */', '#define RSX_FP_COUNT 20']
    with tempfile.TemporaryDirectory() as tmp:
        programs=[('vertex','v',VP)]+[('fragment%d'%i,'f',fragment(i)) for i in range(4)]+[('copy','f',COPY),('display24','f',DISPLAY24)]+[('opaque%d_%d'%(m,d),'f',opaque_fragment(m,d)) for m in range(4) for d in range(2)]+[('upload','f',UPLOAD)]+[('native%d'%m,'f',native_fragment(m)) for m in range(4)]+[('display15','f',DISPLAY15)]
        for name,kind,source in programs:
            src=pathlib.Path(tmp)/(name+'.cg'); binary=src.with_suffix('.bin')
            src.write_text(source)
            # Packed VRAM words require full-precision arithmetic.
            subprocess.run([compiler,'-'+kind,'-Wcg,-bestprecision',str(src),str(binary)],check=True)
            data=binary.read_bytes()
            if len(data)<16: raise RuntimeError('Empty program: '+name)
            output.append('static const unsigned char rsx_'+name+'[] __attribute__((aligned(16))) = {')
            for at in range(0,len(data),16):
                output.append('  '+','.join('0x%02x'%v for v in data[at:at+16])+',')
            output.append('};')
    target=pathlib.Path(target); target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text('\n'.join(output)+'\n')
if __name__=='__main__': main()
