#pragma once
namespace mikudancestudio::antialias {
inline constexpr char kShader[] = R"hlsl(
sampler2D scene : register(s0);
sampler2D history : register(s1);
float4 pixel : register(c0); // inverse dimensions, history weight, unused
float4 bounds : register(c1); // viewport pixel-center UV bounds
float4 readScene(float2 uv) { return tex2D(scene,clamp(uv,bounds.xy,bounds.zw)); }
float luminance(float3 c) { return dot(c,float3(.299,.587,.114)); }
// Directional FXAA: detect local luma contrast, search along the edge, reject
// the wider filter when it crosses the local luma range. Flat areas stay exact.
float4 fxaa(float2 uv : TEXCOORD0) : COLOR0 {
    float4 center=readScene(uv);
    float nw=luminance(readScene(uv+pixel.xy*float2(-1,-1)).rgb);
    float ne=luminance(readScene(uv+pixel.xy*float2(1,-1)).rgb);
    float sw=luminance(readScene(uv+pixel.xy*float2(-1,1)).rgb);
    float se=luminance(readScene(uv+pixel.xy*float2(1,1)).rgb);
    float m=luminance(center.rgb);
    float low=min(m,min(min(nw,ne),min(sw,se)));
    float high=max(m,max(max(nw,ne),max(sw,se)));
    if(high-low < max(.0312,high*.125)) return center;
    float2 direction=float2(-((nw+ne)-(sw+se)),(nw+sw)-(ne+se));
    float reduce=max((nw+ne+sw+se)*(.25*.125),1.0/128.0);
    direction=clamp(direction/(min(abs(direction.x),abs(direction.y))+reduce),-8,8)*pixel.xy;
    float4 a=.5*(readScene(uv+direction*(-1.0/6.0))+readScene(uv+direction*(1.0/6.0)));
    float4 b=a*.5+.25*(readScene(uv-direction*.5)+readScene(uv+direction*.5));
    float l=luminance(b.rgb);
    return l<low || l>high ? a:b;
}
// Static temporal accumulation with a 3x3 neighborhood clamp. The host drops
// history on camera/pose changes, so this does not invent motion vectors for MME.
float4 temporal(float2 uv : TEXCOORD0) : COLOR0 {
    float4 current=readScene(uv), low=current, high=current;
    [unroll] for(int y=-1;y<=1;++y) [unroll] for(int x=-1;x<=1;++x) {
        float4 c=readScene(uv+float2(x,y)*pixel.xy);low=min(low,c);high=max(high,c);
    }
    float4 old=clamp(tex2D(history,uv),low,high);
    return lerp(current,old,pixel.z);
}
)hlsl";
}
