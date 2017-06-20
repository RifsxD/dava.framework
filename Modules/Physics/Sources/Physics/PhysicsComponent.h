#pragma once

#include <Entity/Component.h>
#include <Scene3D/Entity.h>
#include <Reflection/Reflection.h>

#include <Base/BaseTypes.h>

namespace physx
{
class PxActor;
} // namespace physx

namespace DAVA
{
class Entity;
class KeyedArchive;
class SerializationContext;

class PhysicsComponent : public Component
{
public:
    void Serialize(KeyedArchive* archive, SerializationContext* serializationContext) override;
    void Deserialize(KeyedArchive* archive, SerializationContext* serializationContext) override;

    enum eBodyFlags
    {
        VISUALIZE = 0x1,
        DISABLE_GRAVITY = 0x8,
        WAKEUP_SPEEL_NOTIFY = 0x4,
        DISABLE_SIMULATION = 0x2,
    };

    eBodyFlags GetBodyFlags() const;
    void SetBodyFlags(eBodyFlags flags);

    physx::PxActor* GetPxActor() const;

protected:
#if defined(__DAVAENGINE_DEBUG__)
    virtual void CheckActorType() const = 0;
#endif
    virtual void SetPxActor(physx::PxActor* actor);

    physx::PxActor* actor = nullptr;

private:
    friend class PhysicsSystem;
    void ReleasePxActor();

    void UpdateBodyFlags();

    eBodyFlags flags = VISUALIZE;

    DAVA_VIRTUAL_REFLECTION(PhysicsComponent, Component);
};
} // namespace DAVA
