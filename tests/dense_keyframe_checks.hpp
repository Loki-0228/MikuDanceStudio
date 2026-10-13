#pragma once
#include <chrono>

// Diagnostic for a dense 0..500 bake followed by a sparse remainder to 3845.
// Reports track-evaluation time only, not renderer FPS.
void DenseKeyframeChecks() {
    constexpr unsigned count = 400, last = 501;
    auto model = std::make_unique<mdl::ModelRecord>();
    std::vector<mdl::BoneRecord> bones(count);
    std::vector<unsigned> cursors(count);
    std::vector<unsigned char> active(count, 1);
    std::vector<mdl::BoneKey> keys(count * (last + 1));
    model->boneCount = count; model->boneTable = bones.data();
    model->boneKeys = keys.data(); model->boneKeyCursors = cursors.data();
    model->boneTrackActive = active.data();
    auto* raw = reinterpret_cast<unsigned char*>(model.get());
    for (unsigned b=0; b<count; ++b) {
        bones[b].type = mdl::BoneType::RotateMove;
        bones[b].slotIndex = -1;
        for (unsigned f=0; f<=last; ++f) {
            auto& key = keys[f*count+b];
            key.frame = f==last ? 3845 : f;
            key.previous = f ? (f-1)*count+b : 0;
            key.next = f<last ? (f+1)*count+b : 0;
            key.rotation[3] = 1;
            key.position[0] = float(key.frame)*.001f;
            for(int c=0;c<4;++c) {
                key.interpolation[c]=key.interpolation[c+4]=20;
                key.interpolation[c+8]=key.interpolation[c+12]=107;
            }
        }
    }
    for (bool scaled : {false, true}) {
        if (scaled) for (unsigned b=0;b<count;++b) {
            bone_scale::Matrix scale; scale.v[0]=1.2f;
            bone_scale::SetKey(raw,int(b),scale);
        }
        for (int start : {0, 501}) {
            std::fill(active.begin(),active.end(),1);
            for(unsigned b=0;b<count;++b)cursors[b]=b;
            double total=0;
            for(int i=0;i<1000;++i) {
                const float frame=float(start)+i*.5f;
                const auto begin=std::chrono::steady_clock::now();
                AdvanceModelKeyframes(raw,frame/30.f,0);
                total+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
                Check(std::abs(bones[0].trans[0]-frame*.001f)<.00001f,
                      "dense and sparse playback preserve fractional poses");
            }
            std::printf("DENSE_KEYS bones=%u keys=%zu scale=%d start=%d mean_ms=%.6f\n",
                        count,keys.size(),scaled,start,total/1000);
        }
        bone_scale::Forget(raw);
    }
}
