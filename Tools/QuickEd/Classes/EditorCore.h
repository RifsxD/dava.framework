/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/


#ifndef QUICKED_EDITOR_CORE_H
#define QUICKED_EDITOR_CORE_H


#include "Base/Introspection.h"
#include "Project/Project.h"
#include "Base/BaseTypes.h"
#include "Base/Singleton.h"
#include "AssetCache/AssetCacheClient.h"
#include "UI/mainwindow.h"
#include <QObject>

class QAction;
class Document;
class DocumentGroup;
class Project;
class PackageNode;
class SpritesPacker;

namespace DAVA
{
class AssetCacheClient;
}

class EditorCore : public QObject, public DAVA::Singleton<EditorCore>, public DAVA::InspBase
{
    Q_OBJECT
public:
    explicit EditorCore(QObject* parent = nullptr);
    ~EditorCore();
    MainWindow* GetMainWindow() const;
    Project* GetProject() const;
    void Start();

private slots:

    void OnReloadSpritesStarted();
    void OnReloadSpritesFinished();

    void OnProjectPathChanged(const QString& path);
    void OnGLWidgedInitialized();

    void RecentMenu(QAction*);

    void UpdateLanguage();

    void OnRtlChanged(bool isRtl);
    void OnBiDiSupportChanged(bool support);
    void OnGlobalStyleClassesChanged(const QString& classesStr);

    bool CloseProject();
    void OnExit();
    void OnNewProject();

private:
    void OpenProject(const QString& path);

    bool IsUsingAssetCache() const;
    void SetUsingAssetCacheEnabled(bool enabled);

    DAVA::String GetAssetCacheIp() const;
    void SetAssetCacheIp(const DAVA::String& ip);

    DAVA::uint16 GetAssetCachePort() const;
    void SetAssetCachePort(DAVA::uint16 port);

    DAVA::uint64 GetAssetCacheTimeout() const;
    void SetAssetCacheTimeout(DAVA::uint64 timeout);

    std::unique_ptr<SpritesPacker> spritesPacker;
    std::unique_ptr<DAVA::AssetCacheClient> cacheClient;

    Project* project = nullptr;
    DocumentGroup* documentGroup = nullptr;
    std::unique_ptr<MainWindow> mainWindow;

    DAVA::AssetCacheClient::ConnectionParams connectionParams;
    bool assetCacheEnabled;
    REGISTER_PREFERENCES(EditorCore)

public:
    INTROSPECTION(EditorCore,
                  PROPERTY("isUsingAssetCache", "Asset cache/Use asset cache", IsUsingAssetCache, SetUsingAssetCacheEnabled, DAVA::I_VIEW | DAVA::I_EDIT | DAVA::I_PREFERENCE)
                  PROPERTY("assetCacheIp", "Asset cache/Asset Cache IP", GetAssetCacheIp, SetAssetCacheIp, DAVA::I_VIEW | DAVA::I_EDIT | DAVA::I_PREFERENCE)
                  PROPERTY("assetCachePort", "Asset cache/Asset Cache Port", GetAssetCachePort, SetAssetCachePort, DAVA::I_VIEW | DAVA::I_EDIT | DAVA::I_PREFERENCE)
                  PROPERTY("assetCacheTimeout", "Asset cache/Asset Cache Timeout (ms)", GetAssetCacheTimeout, SetAssetCacheTimeout, DAVA::I_VIEW | DAVA::I_EDIT | DAVA::I_PREFERENCE)
                  )

};

inline EditorFontSystem* GetEditorFontSystem()
{
    return EditorCore::Instance()->GetProject()->GetEditorFontSystem();
}

#endif // QUICKED_EDITOR_CORE_H
