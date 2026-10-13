#pragma once
namespace mikudancestudio::antialias {
inline constexpr char kShader[] = R"hlsl(
sampler2D scene : register(s0);
sampler2D history : register(s1);
sampler2D motion : register(s2);
sampler2D previousDepth : register(s3);
float4 pixel : register(c0); // inverse dimensions, history weight, unused
float4 bounds : register(c1); // viewport pixel-center UV bounds
float4 reproject : register(c2); // fallback jitter UV, absolute/relative depth tolerance
float4 readScene(float2 uv) { return tex2D(scene,clamp(uv,bounds.xy,bounds.zw)); }
float luminance(float3 c) { return dot(c,float3(.299,.587,.114)); }
// Directional FXAA: detect local luma contrast, search along the edge, reject
// the wider filter when it crosses the local luma range. Flat areas stay exact.
float4 fxaa(float2 uv : TEXCOORD0) : COLOR0 {
    uv+=reproject.xy;
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
// Actual previous/current geometry positions provide velocity without jitter.
// The history lookup follows that velocity. Linear-depth rejection prevents a
// newly exposed surface from inheriting the occluder's color.
float4 temporal(float2 uv : TEXCOORD0) : COLOR0 {
    float4 current=readScene(uv), low=current, high=current;
    if(pixel.z<=0) return current;
    float4 velocity=tex2D(motion,uv);
    if(velocity.z>0 && velocity.w<=0) return current;
    [unroll] for(int y=-1;y<=1;++y) [unroll] for(int x=-1;x<=1;++x) {
        float2 at=clamp(uv+float2(x,y)*pixel.xy,bounds.xy,bounds.zw);
        float4 c=readScene(at);low=min(low,c);high=max(high,c);
        // Use the nearest visible surface at silhouettes. The jittered sample
        // can land on either side of an edge while referring to the same pixel.
        float4 candidate=tex2D(motion,at);
        if(candidate.z>0 && (velocity.z<=0 || candidate.z<velocity.z)) velocity=candidate;
    }
    float2 previous=uv+velocity.xy;
    if(velocity.z<=0 || velocity.w<=0 || any(previous<bounds.xy) || any(previous>bounds.zw)) return current;
    float tolerance=max(reproject.z,abs(velocity.w)*reproject.w);
    float mismatch=abs(tex2D(previousDepth,previous).z-velocity.w);
    // A one-pixel depth neighborhood covers changed subpixel coverage without
    // accepting unrelated geometry farther from the predicted previous depth.
    [unroll] for(int dy=-1;dy<=1;++dy) [unroll] for(int dx=-1;dx<=1;++dx) {
        float2 at=clamp(previous+float2(dx,dy)*pixel.xy,bounds.xy,bounds.zw);
        float depth=tex2D(previousDepth,at).z;
        if(depth>0) mismatch=min(mismatch,abs(depth-velocity.w));
    }
    if(mismatch>tolerance) return current;
    float4 old=clamp(tex2D(history,previous),low,high);
    return lerp(current,old,pixel.z);
}
)hlsl";
}
