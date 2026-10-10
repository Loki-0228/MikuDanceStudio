// Source-level MMV services. No injection, patches, or solver replacement.
struct PhysicsSnapshot {
    std::vector<MmvPhysicsVertex> vertices;
    std::vector<MmvRigidBinding> bindings;
    std::vector<MmvJointFrames> joints;
};
struct PhysicsFrameSnapshots {
    uint64_t frame=0;
    std::vector<std::unique_ptr<PhysicsSnapshot>> borrowed;
};
std::map<uint64_t,PhysicsFrameSnapshots> physicsSnapshots;
uint32_t snapshotNativePhysics=1;

void MultiplyPhysicsMatrix(float out[16],const float a[16],const float b[16]) {
    float result[16]{};for(int r=0;r<4;++r)for(int c=0;c<4;++c)for(int k=0;k<4;++k)result[r*4+c]+=a[r*4+k]*b[k*4+c];memcpy(out,result,sizeof(result));
}
void RigidBindingMatrix(const mdl::RigidRecord& rigid,float out[16]) {
    float x[16],y[16],z[16];mmv::Identity(x);mmv::Identity(y);mmv::Identity(z);
    const float sx=std::sin(rigid.rotation[0]),cx=std::cos(rigid.rotation[0]);
    const float sy=std::sin(rigid.rotation[1]),cy=std::cos(rigid.rotation[1]);
    const float sz=std::sin(rigid.rotation[2]),cz=std::cos(rigid.rotation[2]);
    x[5]=x[10]=cx;x[6]=sx;x[9]=-sx;y[0]=y[10]=cy;y[2]=-sy;y[8]=sy;
    z[0]=z[5]=cz;z[1]=sz;z[4]=-sz;MultiplyPhysicsMatrix(out,z,x);MultiplyPhysicsMatrix(out,out,y);
    memcpy(out+12,rigid.position,12);
}
int MMV_CALL PhysicsInput(void*,uint64_t objectId,MmvPhysicsInput* output) {
    if(!host||!output||output->structureSize<offsetof(MmvPhysicsInput,frameDeltaSeconds))return MMV_INVALID_ARGUMENT;
    const auto outputSize=std::min<size_t>(output->structureSize,sizeof(*output));
    unsigned char* source=nullptr;
    for(int i=0;i<kModelSlotCount;++i)if(reinterpret_cast<uintptr_t>(host->ModelSlot(i))==objectId){source=host->ModelSlot(i);break;}
    if(!source)return MMV_UNSUPPORTED;
    try {
        const auto& model=*mdl::Mdl(source);
        // Model identity and record counts do not identify the current morph,
        // body properties or joint frames. Read their live values on every call.
        if((model.vertexCount && (model.physicsMode==2?!model.pmxVertices:!model.rawVertices)) ||
           (model.rigidCount && !model.rigidTable) || (model.jointCount && !model.jointTable))
            return MMV_FAILED;
        auto data=std::make_unique<PhysicsSnapshot>();data->vertices.resize(model.vertexCount);
        for(uint32_t i=0;i<model.vertexCount;++i){
            const float* position=model.physicsMode==2?model.pmxVertices[i].position:model.rawVertices[i].position;
            memcpy(data->vertices[i].position,position,6*sizeof(float));
        }
        data->bindings.resize(model.rigidCount);
        for(uint32_t i=0;i<model.rigidCount;++i){const auto& rigid=model.rigidTable[i];auto& binding=data->bindings[i];
            RigidBindingMatrix(rigid,binding.boneToBody);
            if(auto* body=static_cast<btRigidBody*>(rigid.body)){binding.valid=1;
                for(int axis=0;axis<3;++axis)if(body->getInvInertiaDiagLocal()[axis]>0)binding.inertia[axis]=1.f/body->getInvInertiaDiagLocal()[axis];}
        }
        data->joints.resize(model.jointCount);
        std::map<int,btGeneric6DofConstraint*> constraints;
        if(model.scenePtr&&model.scenePtr->world)for(int i=0;i<model.scenePtr->world->getNumConstraints();++i){
            auto* c=model.scenePtr->world->getConstraint(i);
            if(auto* joint=dynamic_cast<btGeneric6DofConstraint*>(c))constraints[c->getUserConstraintId()]=joint;
        }
        for(uint32_t i=0;i<model.jointCount;++i){const auto found=constraints.find(model.jointTable[i].constraint);
            if(found==constraints.end())continue;auto& frames=data->joints[i];frames.valid=1;
            Matrix(found->second->getFrameOffsetA(),frames.frameA);Matrix(found->second->getFrameOffsetB(),frames.frameB);
        }
        MmvPhysicsInput next{};next.structureSize=uint32_t(outputSize);next.nativePhysicsEnabled=snapshotNativePhysics;next.objectId=objectId;
        if(outputSize>=sizeof(next)){next.flags=MMV_PHYSICS_TIMING_VALID|(host->PlaybackActive()?MMV_PHYSICS_PLAYING:0)|(host->FrameStepPlayback()?MMV_PHYSICS_FRAME_STEP:0);
            next.frameDeltaSeconds=std::max(0.f,host->DeltaTime());}
        if(model.scenePtr&&model.scenePtr->world){const auto gravity=model.scenePtr->world->getGravity();for(int c=0;c<3;++c)next.gravity[c]=gravity[c];}
        else next.gravity[1]=-98.f;
        next.vertices=data->vertices.data();next.vertexCount=uint32_t(data->vertices.size());
        next.rigidBindings=data->bindings.data();next.rigidCount=uint32_t(data->bindings.size());
        next.jointFrames=data->joints.data();next.jointCount=uint32_t(data->joints.size());
        // The plugin may query one object more than once during evaluate.
        // Keep every published array for this frame, even when counts grow.
        // Render advances frameNumber before entering the synchronous plugin;
        // existing scene/object removal paths erase this owner's whole entry.
        auto& cached=physicsSnapshots[objectId];
        if(cached.frame!=frameNumber){
            // Allocate before retiring the previous frame, so a failed query
            // never replaces a previously published snapshot with partial data.
            std::vector<std::unique_ptr<PhysicsSnapshot>> fresh;
            fresh.push_back(std::move(data));
            cached.borrowed.swap(fresh);cached.frame=frameNumber;
        }else cached.borrowed.push_back(std::move(data));
        memcpy(output,&next,outputSize);return MMV_OK;
    }catch(...){return MMV_FAILED;}
}
