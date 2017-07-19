#pragma once

#include "Base/BaseTypes.h"
#include "Base/FastName.h"
#include "Reflection/Reflection.h"
#include "Entity/Component.h"

namespace DAVA
{
class AnimationClip;
class MotionSystem;
class SkeletonAnimation;
class SkeletonComponent;
class MotionComponent : public Component
{
public:
    static const FastName EVENT_SINGLE_ANIMATION_STARTED;
    static const FastName EVENT_SINGLE_ANIMATION_ENDED;

    IMPLEMENT_COMPONENT_TYPE(MOTION_COMPONENT);

    MotionComponent();
    ~MotionComponent();

    Component* Clone(Entity* toEntity) override;
    void Serialize(KeyedArchive* archive, SerializationContext* serializationContext) override;
    void Deserialize(KeyedArchive* archive, SerializationContext* serializationContext) override;

private:
    class SimpleMotion
    {
    public:
        SimpleMotion(MotionComponent* component);
        ~SimpleMotion();

        void BindSkeleton(SkeletonComponent* skeleton);
        void Start();
        void Stop();
        void Update(float32 dTime);

        bool IsPlaying() const;
        bool IsFinished() const;

        const SkeletonAnimation* GetAnimation() const;

    private:
        const FilePath& GetAnimationPath() const;
        void SetAnimationPath(const FilePath& path);

        uint32 GetRepeatsCount() const;
        void SetRepeatsCount(uint32 count);

        //Serializable
        FilePath animationPath;
        uint32 repeatsCount = 0;

        //Runtime
        AnimationClip* animationClip = nullptr;
        SkeletonAnimation* skeletonAnimation = nullptr;

        bool isPlaying = false;
        uint32 repeatsLeft = 0;
        float32 currentAnimationTime = 0.f;

        MotionComponent* component = nullptr; //weak-pointer

        friend class MotionComponent;

        DAVA_REFLECTION(SimpleMotion);
    };

    SimpleMotion* simpleMotion = nullptr;

    DAVA_VIRTUAL_REFLECTION(MotionComponent, Component);

public:
    INTROSPECTION_EXTEND(MotionComponent, Component, nullptr);

    friend class MotionSystem;
};

} //ns
