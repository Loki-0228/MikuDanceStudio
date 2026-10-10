struct Vertex {
    float3 position,normal;float2 uv;float4 additionalUv[4];float3 edgePosition;uint edgeColor;
};
StructuredBuffer<Vertex> vertices:register(t0);
StructuredBuffer<uint> sourceIndices:register(t1);
RWStructuredBuffer<uint2> keys:register(u0);
RWStructuredBuffer<uint> sortedIndices:register(u1);
cbuffer Sort:register(b0) {
    uint firstIndex,triangleCount,paddedCount,mode;
    uint k,j,reserved0,reserved1;
    float4 depthTransform;
};
bool before(uint2 a,uint2 b) {
    float da=asfloat(a.x),db=asfloat(b.x);
    return da>db || (da==db && a.y<b.y);
}
[numthreads(64,1,1)]
void main(uint3 tid:SV_DispatchThreadID) {
    uint i=tid.x;if(i>=paddedCount)return;
    if(mode==0) {
        precise float depth=0;
        if(i<triangleCount) {
            [unroll]for(uint n=0;n<3;++n) {
                float3 p=vertices[sourceIndices[firstIndex+3*i+n]].position;
                precise float d=p.x*depthTransform.x+p.y*depthTransform.y+p.z*depthTransform.z+depthTransform.w;
                depth+=d/3.;
            }
        } else depth=asfloat(0xff800000);
        keys[i]=uint2(asuint(depth),i);return;
    }
    if(mode==1) {
        uint other=i^j;if(other<=i || other>=paddedCount)return;
        uint2 a=keys[i],b=keys[other];
        bool forward=(i&k)==0;
        if(forward ? before(b,a) : before(a,b)) {keys[i]=b;keys[other]=a;}
        return;
    }
    if(i<triangleCount) {
        uint original=keys[i].y;
        [unroll]for(uint n=0;n<3;++n)sortedIndices[firstIndex+3*i+n]=sourceIndices[firstIndex+3*original+n];
    }
}
