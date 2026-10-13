#pragma once
namespace mikudancestudio::antialias {
// Official MEDIUM/HIGH presets and T2x, with color edges for MMD's colored lines.
// The algorithm and lookup data retain their license in third_party.
inline constexpr char kSmaaShader[] = R"hlsl(
float4 smaaMetrics : register(c0);
float4 smaaBounds : register(c1);
float4 smaaSubsamples : register(c2);
float4 smaaTemporalState : register(c3); // valid history, absolute/relative depth tolerance
float4 smaaOutputOffset : register(c4); // undo jitter only after motion capture falls back
#define SMAA_RT_METRICS smaaMetrics
#define SMAA_HLSL_3 1
#ifndef MMS_SMAA_ENHANCED
#define MMS_SMAA_ENHANCED 1
#endif
#ifndef MMS_SMAA_TEMPORAL
#define MMS_SMAA_TEMPORAL 0
#endif
#if MMS_SMAA_ENHANCED
#define SMAA_PRESET_HIGH 1
#else
#define SMAA_PRESET_MEDIUM 1
#endif
#define SMAA_REPROJECTION MMS_SMAA_TEMPORAL
// The motion buffer stores previous-current, opposite to the upstream convention.
#define SMAA_DECODE_VELOCITY(sample) (-(sample).rg)
#define SMAA_AREATEX_SELECT(sample) sample.rg
)hlsl"
#include "smaa_reference.hpp"
R"hlsl(
sampler2D smaaInput : register(s0);
sampler2D smaaSecondary : register(s1);
sampler2D smaaSearch : register(s2);
sampler2D smaaPreviousDepth : register(s3);
sampler2D smaaOriginal : register(s4);
sampler2D smaaBlendWeights : register(s5);
float4 smaaEdges(float2 uv : TEXCOORD0) : COLOR0 {
    float4 offsets[3];
    SMAAEdgeDetectionVS(uv,offsets);
    // Only the scene viewport is initialized in the full-size render target.
    [unroll] for(int i=0;i<3;++i) {
        offsets[i].xy=clamp(offsets[i].xy,smaaBounds.xy,smaaBounds.zw);
        offsets[i].zw=clamp(offsets[i].zw,smaaBounds.xy,smaaBounds.zw);
    }
    return float4(SMAAColorEdgeDetectionPS(uv,offsets,smaaInput),0,0);
}
float4 smaaWeights(float2 uv : TEXCOORD0) : COLOR0 {
    float2 pixel;float4 offsets[3];
    SMAABlendingWeightCalculationVS(uv,pixel,offsets);
    return SMAABlendingWeightCalculationPS(uv,pixel,offsets,smaaInput,smaaSecondary,smaaSearch,smaaSubsamples);
}
float4 smaaResolve(float2 uv : TEXCOORD0) : COLOR0 {
    uv=clamp(uv+smaaOutputOffset.xy,smaaBounds.xy,smaaBounds.zw);
    // Preserve the outermost scene pixels; never blend undefined editor pixels
    // into an inset viewport or into a cropped/export render target.
    if(any(uv<=smaaBounds.xy+smaaMetrics.xy*.01) || any(uv>=smaaBounds.zw-smaaMetrics.xy*.01)) {
        float4 color=tex2D(smaaInput,clamp(uv,smaaBounds.xy,smaaBounds.zw));
        #if MMS_SMAA_TEMPORAL
        color.a=sqrt(5.0*length(tex2D(smaaSearch,uv).rg));
        #endif
        return color;
    }
    float4 offset;SMAANeighborhoodBlendingVS(uv,offset);
    return SMAANeighborhoodBlendingPS(uv,offset,smaaInput,smaaSecondary
        #if MMS_SMAA_TEMPORAL
        ,smaaSearch
        #endif
    );
}
#if MMS_SMAA_TEMPORAL
// Upstream packs velocity into neighborhood alpha for its temporal rejection.
// Recover the current spatially filtered alpha rather than exposing that packing
// to transparent image/video outputs.
float smaaOutputAlpha(float2 uv) {
    float4 offset;SMAANeighborhoodBlendingVS(uv,offset);
    float4 a;
    a.x=tex2D(smaaBlendWeights,offset.xy).a;
    a.y=tex2D(smaaBlendWeights,offset.zw).g;
    a.wz=tex2D(smaaBlendWeights,uv).xz;
    if(dot(a,1)<1e-5 || any(uv<=smaaBounds.xy+smaaMetrics.xy*.01) || any(uv>=smaaBounds.zw-smaaMetrics.xy*.01))
        return tex2D(smaaOriginal,uv).a;
    bool horizontal=max(a.x,a.z)>max(a.y,a.w);
    float2 weight=horizontal?a.xz:a.yw;
    weight/=dot(weight,1);
    float4 blend=horizontal?float4(a.x,0,a.z,0):float4(0,a.y,0,a.w);
    float4 at=mad(blend,float4(smaaMetrics.xy,-smaaMetrics.xy),uv.xyxy);
    return weight.x*tex2D(smaaOriginal,clamp(at.xy,smaaBounds.xy,smaaBounds.zw)).a+
        weight.y*tex2D(smaaOriginal,clamp(at.zw,smaaBounds.xy,smaaBounds.zw)).a;
}
float4 smaaTemporal(float2 uv : TEXCOORD0) : COLOR0 {
    float4 current=tex2D(smaaInput,uv);
    float alpha=smaaOutputAlpha(uv);
    current.a=alpha;
    if(smaaTemporalState.x<=0)return current;
    float4 velocity=tex2D(smaaSearch,uv);
    float2 previous=uv+velocity.xy;
    if(velocity.z<=0 || velocity.w<=0 || any(previous<smaaBounds.xy) || any(previous>smaaBounds.zw))return current;
    float tolerance=max(smaaTemporalState.y,abs(velocity.w)*smaaTemporalState.z);
    float mismatch=abs(tex2D(smaaPreviousDepth,previous).z-velocity.w);
    [unroll] for(int dy=-1;dy<=1;++dy) [unroll] for(int dx=-1;dx<=1;++dx) {
        float2 at=clamp(previous+float2(dx,dy)*smaaMetrics.xy,smaaBounds.xy,smaaBounds.zw);
        float depth=tex2D(smaaPreviousDepth,at).z;
        if(depth>0)mismatch=min(mismatch,abs(depth-velocity.w));
    }
    if(mismatch>tolerance)return current;
    // Keep exactly the upstream two-frame resolve and velocity-change weight.
    // History is the previous neighborhood result, never the resolved result.
    float4 result=SMAAResolvePS(uv,smaaInput,smaaSecondary,smaaSearch);
    result.a=alpha;
    return result;
}
#endif
)hlsl";
}
