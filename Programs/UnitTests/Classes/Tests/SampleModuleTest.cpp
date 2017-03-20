#include "UnitTests/UnitTests.h"
#include "Engine/Engine.h"
#include "FileSystem/FilePath.h"
#include "FileSystem/FileSystem.h"
#include "PluginManager/PluginManager.h"
#include "Entity/ComponentManager.h"

#if !defined(__DAVAENGINE_ANDROID__)

#include "SampleModule.h"

using namespace DAVA;

DAVA_TESTCLASS (SampleModuleTest)
{
    DAVA_TEST (CheckStatus)
    {
        const ModuleManager& moduleManager = *GetEngineContext()->moduleManager;
        SampleModule* sampleModule = moduleManager.GetModule<SampleModule>();

        auto statusList = sampleModule->StatusList();

        TEST_VERIFY(statusList.size() == 2);
        TEST_VERIFY(statusList[0] == SampleModule::ES_UNKNOWN);
        TEST_VERIFY(statusList[1] == SampleModule::ES_INIT);
    }

    DAVA_TEST (ModuleUIComponent)
    {
        UIComponent* c = UIComponent::CreateByType(Type::Instance<SampleModuleUIComponent>());
        TEST_VERIFY(dynamic_cast<SampleModuleUIComponent*>(c));
        c->Release();
    }

    DAVA_TEST (PluginUIComponent)
    {
        PluginManager* mm = GetEngineContext()->pluginManager;
        FileSystem* fs = GetEngineContext()->fileSystem;

        FilePath pluginDir = fs->GetPluginDirectory();

        Vector<FilePath> pluginsList;
        pluginsList = mm->GetPlugins(pluginDir, PluginManager::Auto);

        for (auto& path : pluginsList)
        {
            const PluginDescriptor* pluginDescriptor = mm->LoadPlugin(path);
            DVASSERT(pluginDescriptor != nullptr);
        }

        auto& container = GetEngineContext()->componentManager->GetRegisteredTypes();
    }
};

#endif // !defined(__DAVAENGINE_ANDROID__)
