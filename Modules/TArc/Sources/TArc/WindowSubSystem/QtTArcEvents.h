#pragma once

#include <Base/BaseTypes.h>

#include <QEvent>

#define QT_EVENT_TYPE(x) (static_cast<QEvent::Type>(x))
namespace DAVA
{
namespace TArc
{
enum class EventsTable : DAVA::int32
{
    DelayedExecute = QEvent::User + 100, // 100 events is reserved for PlatformQt implementation
    End
};
} // namespace TArc
} // namespace DAVA