struct Vertex {
    float3 position,normal;float2 uv;float4 additionalUv[4];float3 edgePosition;uint edgeColor;
};
StructuredBuffer<Vertex> vertices : register(t0);
Texture2D diffuseMap : register(t1);
Texture2D sphereMap : register(t2);
Texture2D toonMap : register(t3);
SamplerState linearWrap : register(s0);
SamplerState linearClamp : register(s1);
cbuffer Draw : register(b0) {
    row_major float4x4 worldViewProjection,worldView;
    float4 lightDirection,lightColor,diffuse,ambient,specular,camera,flags;
};
struct Interpolated {
    float4 position:SV_Position;
    float3 normal:NORMAL;float3 eye:TEXCOORD2;
    float2 uv:TEXCOORD0;float2 additionalUv:TEXCOORD1;
    float4 edgeColor:COLOR0;
};
Interpolated vs(uint index:SV_VertexID) {
    Vertex v=vertices[index];Interpolated o;
    float3 p=flags.x!=0 ? v.edgePosition : v.position;
    o.position=mul(float4(p,1),worldViewProjection);
    o.normal=mul(float4(v.normal,0),worldView).xyz;
    o.eye=-mul(float4(p,1),worldView).xyz;
    o.uv=v.uv;o.additionalUv=v.additionalUv[0].xy;
    o.edgeColor=float4((v.edgeColor>>16)&255,(v.edgeColor>>8)&255,v.edgeColor&255,v.edgeColor>>24)/255.;
    return o;
}
float4 ps(Interpolated i):SV_Target {
    if(flags.x!=0) {clip(i.edgeColor.a-1./255.);return i.edgeColor;}
    float3 normal=normalize(i.normal);
    float3 direction=normalize(mul(float4(lightDirection.xyz,0),worldView).xyz);
    float amount=dot(normal,-direction)*.5+.5;
    float3 toon=toonMap.Sample(linearClamp,float2(.5,1-saturate(amount))).rgb;
    float4 base=diffuseMap.Sample(linearWrap,i.uv)*diffuse;
    float3 color=saturate((diffuse.rgb+ambient.rgb)*lightColor.rgb)*toon;
    color*=diffuseMap.Sample(linearWrap,i.uv).rgb;
    if(flags.y!=0) {
        float2 uv=flags.y==3 ? i.additionalUv : float2(normal.x*.5+.5,.5-normal.y*.5);
        float3 sphere=sphereMap.Sample(linearWrap,uv).rgb;
        color=flags.y==2 ? color+sphere : color*sphere;
    }
    color+=specular.rgb*lightColor.rgb*pow(saturate(dot(normal,normalize(-direction+normalize(i.eye)))),max(specular.w,.0001));
    clip(base.a-1./255.); return float4(color,base.a);
}
