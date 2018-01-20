#pragma once

#include "TArc/Core/Private/CoreInterface.h"
#include "TArc/WindowSubSystem/UI.h"

#include <Reflection/ReflectionRegistrator.h>

namespace DAVA
{
class DataContext;
class ContextAccessor;
class WindowKey;

class ClientModule : public ReflectionBase
{
public:
    virtual ~ClientModule() = default;

protected:
    virtual void OnContextCreated(DataContext* context)
    {
    }
    virtual void OnContextDeleted(DataContext* context)
    {
    }
    virtual void OnContextWillBeChanged(DataContext* current, DataContext* newOne)
    {
    }
    virtual void OnContextWasChanged(DataContext* current, DataContext* oldOne)
    {
    }
    virtual void OnWindowClosed(const WindowKey& key)
    {
    }

    virtual void OnInterfaceRegistered(const Type* interfaceType)
    {
    }

    virtual void OnBeforeInterfaceUnregistered(const Type* interfaceType)
    {
    }

    virtual void PostInit() = 0;
    ContextAccessor* GetAccessor();
    const ContextAccessor* GetAccessor() const;

    UI* GetUI();
    OperationInvoker* GetInvoker();

    template <typename Ret, typename Cls, typename... Args>
    void RegisterOperation(DAVA::uint32 operationID, Cls* object, Ret (Cls::*fn)(Args...) const);

    template <typename Ret, typename Cls, typename... Args>
    void RegisterOperation(DAVA::uint32 operationID, Cls* object, Ret (Cls::*fn)(Args...));

    template <typename... Args>
    void InvokeOperation(DAVA::uint32 operationId, const Args&... args);

    template <typename TInterface>
    void RegisterInterface(TInterface* interface);

    template <typename TInterface>
    TInterface* QueryInterface() const;

private:
    void Init(CoreInterface* coreInterface, std::unique_ptr<UI>&& ui);

private:
    friend class Core;
    friend class ControllerModule;

    CoreInterface* coreInterface = nullptr;
    std::unique_ptr<UI> ui;

    DAVA_VIRTUAL_REFLECTION_IN_PLACE(ClientModule)
    {
    }
};

template <typename Ret, typename Cls, typename... Args>
inline void ClientModule::RegisterOperation(uint32 operationID, Cls* object, Ret (Cls::*fn)(Args...) const)
{
    coreInterface->RegisterOperation(operationID, AnyFn(fn).BindThis(object));
}

template <typename Ret, typename Cls, typename... Args>
inline void ClientModule::RegisterOperation(uint32 operationID, Cls* object, Ret (Cls::*fn)(Args...))
{
    coreInterface->RegisterOperation(operationID, AnyFn(fn).BindThis(object));
}

template <typename... Args>
inline void ClientModule::InvokeOperation(uint32 operationId, const Args&... args)
{
    coreInterface->Invoke(operationId, args...);
}

template <typename TInterface>
inline void ClientModule::RegisterInterface(TInterface* interface)
{
    coreInterface->RegisterInterface(this, Type::Instance<TInterface>(), interface);
}

template <typename TInterface>
inline TInterface* ClientModule::QueryInterface() const
{
    Any result = coreInterface->QueryInterface(Type::Instance<TInterface>());
    return result.Get<TInterface*>();
}
} // namespace DAVA
