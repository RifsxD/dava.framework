#pragma once

#include "Base/BaseTypes.h"

#if defined(__DAVAENGINE_WIN32__)

#include "Notification/Private/LocalNotificationImpl.h"

namespace DAVA
{
class LocalNotificationNotImplemented : public LocalNotificationImpl
{
public:
    LocalNotificationNotImplemented(const String& _id);
    ~LocalNotificationNotImplemented() override;

    void SetAction(const WideString& action) override;
    void Hide() override;
    void ShowText(const WideString& title, const WideString& text, bool useSound) override;
    void ShowProgress(const WideString& title, const WideString& text, uint32 total, uint32 progress, bool useSound) override;
    void PostDelayedNotification(const WideString& title, const WideString& text, int delaySeconds, bool useSound) override;
    void RemoveAllDelayedNotifications() override;
};

LocalNotificationNotImplemented::LocalNotificationNotImplemented(const String& _id)
{
    notificationId = _id;
}

LocalNotificationNotImplemented::~LocalNotificationNotImplemented()
{
}

void LocalNotificationNotImplemented::SetAction(const WideString& action)
{
}

void LocalNotificationNotImplemented::Hide()
{
}
void LocalNotificationNotImplemented::ShowText(const WideString& title, const WideString& text, bool useSound)
{
}
void LocalNotificationNotImplemented::ShowProgress(const WideString& title, const WideString& text, uint32 total, uint32 progress, bool useSound)
{
}

LocalNotificationImpl* LocalNotificationImpl::Create(const String& _id)
{
    return new LocalNotificationNotImplemented(_id);
}

void LocalNotificationNotImplemented::PostDelayedNotification(const WideString& title, const WideString& text, int delaySeconds, bool useSound)
{
}

void LocalNotificationNotImplemented::RemoveAllDelayedNotifications()
{
}
} // namespace DAVA

#endif // defined(__DAVAENGINE_WIN32__)
