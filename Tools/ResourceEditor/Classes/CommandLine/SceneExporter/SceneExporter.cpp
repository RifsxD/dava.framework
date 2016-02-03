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

#include "CommandLine/SceneExporter/SceneExporter.h"

#include "Debug/Stats.h"
#include "FileSystem/FilePath.h"
#include "FileSystem/FileSystem.h"
#include "Functional/Function.h"
#include "Platform/Process.h"
#include "Platform/SystemTimer.h"
#include "Render/GPUFamilyDescriptor.h"
#include "Render/TextureDescriptor.h"
#include "Render/Highlevel/Heightmap.h"
#include "Render/Highlevel/Landscape.h"
#include "Scene3D/Components/ComponentHelpers.h"
#include "Scene3D/SceneFile/VersionInfo.h"
#include "Utils/StringUtils.h"
#include "Utils/MD5.h"

#include "TextureCompression/TextureConverter.h"

#include "StringConstants.h"
#include "Scene/SceneHelper.h"



using namespace DAVA;

namespace SceneExporterCache
{
using CachedFiles = List<FilePath>;

const uint32 EXPORTER_VERSION = 1;
const uint32 LINKS_PARSER_VERSION = 1;
const String LINKS_NAME = "links.txt";

void CalculateSceneKey(const FilePath& scenePathname, const String& sceneLink, AssetCache::CacheItemKey& key, uint32 optimize)
{
    { //calculate digest for scene file
        MD5::MD5Digest fileDigest;
        MD5::ForFile(scenePathname, fileDigest);

        Memcpy(key.data(), fileDigest.digest.data(), MD5::MD5Digest::DIGEST_SIZE);
    }

    { //calculate digest for params
        MD5::MD5Digest sceneParamsDigest;
        String params = "ResourceEditor";
        params += Format("Pathname: %s", sceneLink.c_str());
        params += Format("SceneFileVersion: %d", SCENE_FILE_CURRENT_VERSION);
        params += Format("ExporterVersion: %u", EXPORTER_VERSION);
        params += Format("LinksParserVersion: %u", LINKS_PARSER_VERSION);
        params += Format("Optimized: %u", optimize);
        //quality.yaml hash?

        MD5::ForData(reinterpret_cast<const uint8*>(params.data()), static_cast<uint32>(params.size()), sceneParamsDigest);
        Memcpy(key.data() + MD5::MD5Digest::DIGEST_SIZE, sceneParamsDigest.digest.data(), MD5::MD5Digest::DIGEST_SIZE);
    }
}

void AddConnectionParamsToParams(Vector<String>& arguments, const SceneExporter::ClientConnectionParams& assetCacheParams)
{
    arguments.push_back("-ip");
    arguments.push_back(assetCacheParams.ip);

    arguments.push_back("-p");
    arguments.push_back(assetCacheParams.port);

    arguments.push_back("-t");
    arguments.push_back(Format("%u", assetCacheParams.timeout));
}

bool GetFromCache(const FilePath& outFolderPath, const AssetCache::CacheItemKey& key, const SceneExporter::ClientConnectionParams& assetCacheParams)
{
    DVASSERT(assetCacheParams.enabled);
    DVASSERT(outFolderPath.IsDirectoryPathname());

    Vector<String> arguments;
    arguments.push_back("get");

    arguments.push_back("-h");
    arguments.push_back(AssetCache::KeyToString(key));

    arguments.push_back("-f");
    arguments.push_back(outFolderPath.GetAbsolutePathname());

    AddConnectionParamsToParams(arguments, assetCacheParams);

    FilePath cacheClientTool = "~res:/AssetCacheClient";
    FilePath workingDir = FileSystem::Instance()->GetCurrentWorkingDirectory();
    SCOPE_EXIT
    {
        FileSystem::Instance()->SetCurrentWorkingDirectory(workingDir);
    };
    FileSystem::Instance()->SetCurrentWorkingDirectory(cacheClientTool.GetDirectory());

    uint64 getTime = SystemTimer::Instance()->AbsoluteMS();
    Process cacheClient(cacheClientTool, arguments);
    if (cacheClient.Run(false))
    {
        cacheClient.Wait();

        int32 exitCode = cacheClient.GetExitCode();
        getTime = SystemTimer::Instance()->AbsoluteMS() - getTime;

        if (exitCode == AssetCache::ERROR_OK)
        {
            Logger::FrameworkDebug("[%s - %.2lf secs] - GOT FROM CACHE", outFolderPath.GetAbsolutePathname().c_str(), (float64)(getTime) / 1000.0f);
            return true;
        }
        else
        {
//             Logger::Info("[%s - %.2lf secs] - attempted to retrieve from cache, result code %d (%s)",
//                          outFolderPath.GetAbsolutePathname().c_str(),
//                          (float64)(getTime) / 1000.0f,
//                          exitCode,
//                          AssetCache::GetExitCodeString(exitCode).c_str());

            const String& procOutput = cacheClient.GetOutput();
            if (!procOutput.empty())
            {
                Logger::FrameworkDebug("\nCacheClientLog: %s", procOutput.c_str());
            }

            return false;
        }
    }
    else
    {
        Logger::Warning("Can't run process 'AssetCacheClient'", cacheClientTool.GetAbsolutePathname().c_str());
        return false;
    }

    return false;
}

bool AddToCache(const CachedFiles& filesForCaching, const AssetCache::CacheItemKey& key, const SceneExporter::ClientConnectionParams& assetCacheParams)
{
    DVASSERT(assetCacheParams.enabled);
    DVASSERT(filesForCaching.empty() == false);

    Vector<String> arguments;
    arguments.push_back("add");

    arguments.push_back("-h");
    arguments.push_back(AssetCache::KeyToString(key));

    String fileListString;
    for (const auto& path : filesForCaching)
    {
        fileListString += (path.GetAbsolutePathname() + ',');
    }

    if (!fileListString.empty())
    {
        fileListString.pop_back();
    }

    arguments.push_back("-f");
    arguments.push_back(fileListString);

    AddConnectionParamsToParams(arguments, assetCacheParams);

    FilePath cacheClientTool = "~res:/AssetCacheClient";
    FilePath workingDir = FileSystem::Instance()->GetCurrentWorkingDirectory();
    SCOPE_EXIT
    {
        FileSystem::Instance()->SetCurrentWorkingDirectory(workingDir);
    };
    FileSystem::Instance()->SetCurrentWorkingDirectory(cacheClientTool.GetDirectory());

    uint64 getTime = SystemTimer::Instance()->AbsoluteMS();
    Process cacheClient(cacheClientTool, arguments);
    if (cacheClient.Run(false))
    {
        cacheClient.Wait();

        int32 exitCode = cacheClient.GetExitCode();
        getTime = SystemTimer::Instance()->AbsoluteMS() - getTime;

        if (exitCode == AssetCache::ERROR_OK)
        {
            Logger::FrameworkDebug("[%s - %.2lf secs] - ADDED TO CACHE", fileListString.c_str(), (float64)(getTime) / 1000.0f);
            return true;
        }
        else
        {
//             Logger::Warning("[%s - %.2lf secs] - attempted to add to cache, result code %d (%s)", fileListString.c_str(),
//                             (float64)(getTime) / 1000.0f,
//                             exitCode,
//                             AssetCache::GetExitCodeString(exitCode).c_str());
            const String& procOutput = cacheClient.GetOutput();
            if (!procOutput.empty())
            {
                Logger::FrameworkDebug("\nCacheClientLog: %s", procOutput.c_str());
            }

            return false;
        }
    }
    else
    {
        Logger::Warning("Can't run process '%s'", cacheClientTool.GetAbsolutePathname().c_str());
        return false;
    }

    return false;
}

} //namespace SceneExporterCache

namespace SceneExporterInternal
{
void SaveExportedObjects(const FilePath& linkPathname, const SceneExporter::ExportedObjectCollection& exportedObjects)
{
    ScopedPtr<File> linksFile(File::Create(linkPathname, File::CREATE | File::WRITE));
    if (linksFile)
    {
        linksFile->WriteLine(Format("%d", static_cast<int32>(exportedObjects.size())));
        for (const auto& object : exportedObjects)
        {
            linksFile->WriteLine(Format("%d,%s", object.type, object.relativePathname.c_str()));
        }
    }
    else
    {
        Logger::Error("Cannot open file with links: %s", linkPathname.GetAbsolutePathname().c_str());
    }
}

void LoadExportedObjects(const FilePath& linkPathname, SceneExporter::ExportedObjectCollection& exportedObjects)
{
    ScopedPtr<File> linksFile(File::Create(linkPathname, File::OPEN | File::READ));
    if (linksFile)
    {
        String sizeStr = linksFile->ReadLine();
        int32 size = 0;
        int32 number = sscanf(sizeStr.c_str(), "%d", &size);
        if (size > 0 && number == 1)
        {
            exportedObjects.reserve(size);
            while (size--)
            {
                if (linksFile->IsEof())
                {
                    Logger::Warning("Reading of file stopped by EOF: %s", linkPathname.GetAbsolutePathname().c_str());
                    break;
                }

                String formatedString = linksFile->ReadLine();
                if (formatedString.empty())
                {
                    Logger::Warning("Reading of file stopped by empty string: %s", linkPathname.GetAbsolutePathname().c_str());
                    break;
                }

                auto dividerPos = formatedString.find(',', 1); //skip first number
                DVASSERT(dividerPos != String::npos);

                exportedObjects.emplace_back(static_cast<SceneExporter::eExportedObjectType>(atoi(formatedString.substr(0, dividerPos).c_str())), formatedString.substr(dividerPos + 1));
            }
        }
        else
        {
            Logger::Error("Cannot open read size value from file: %s", linkPathname.GetAbsolutePathname().c_str());
        }
    }
    else
    {
        Logger::Error("Cannot open file with links: %s", linkPathname.GetAbsolutePathname().c_str());
    }
}

inline bool IsEditorEntity(Entity* entity)
{
    const String::size_type pos = entity->GetName().find(ResourceEditor::EDITOR_BASE);
    return (String::npos != pos);
}

void RemoveEditorCustomProperties(Entity* entity)
{
    //    "editor.dynamiclight.enable";
    //    "editor.donotremove";
    //
    //    "editor.referenceToOwner";
    //    "editor.isSolid";
    //    "editor.isLocked";
    //    "editor.designerName"
    //    "editor.modificationData"
    //    "editor.staticlight.enable";
    //    "editor.staticlight.used"
    //    "editor.staticlight.castshadows";
    //    "editor.staticlight.receiveshadows";
    //    "editor.staticlight.falloffcutoff"
    //    "editor.staticlight.falloffexponent"
    //    "editor.staticlight.shadowangle"
    //    "editor.staticlight.shadowsamples"
    //    "editor.staticlight.shadowradius"
    //    "editor.intensity"

    KeyedArchive* props = GetCustomPropertiesArchieve(entity);
    if (props)
    {
        const KeyedArchive::UnderlyingMap propsMap = props->GetArchieveData();

        for (auto& it : propsMap)
        {
            const String& key = it.first;

            if (key.find(ResourceEditor::EDITOR_BASE) == 0)
            {
                if ((key != ResourceEditor::EDITOR_DO_NOT_REMOVE) && (key != ResourceEditor::EDITOR_DYNAMIC_LIGHT_ENABLE))
                {
                    props->DeleteKey(key);
                }
            }
        }

        if (props->Count() == 0)
        {
            entity->RemoveComponent(DAVA::Component::CUSTOM_PROPERTIES_COMPONENT);
        }
    }
}

void PrepareSceneToExport(Scene* scene, bool removeCustomProperties)
{
    //Remove scene nodes
    Vector<Entity*> entities;
    scene->GetChildNodes(entities);

    for (auto& entity : entities)
    {
        bool needRemove = IsEditorEntity(entity);
        if (needRemove)
        {
            //remove nodes from hierarchy
            DVASSERT(entity->GetParent() != nullptr);
            entity->GetParent()->RemoveNode(entity);
        }
        else if (removeCustomProperties)
        {
            RemoveEditorCustomProperties(entity);
        }
    }
}

void CollectHeightmapPathname(Scene* scene, const FilePath& dataSourceFolder, SceneExporter::ExportedObjectCollection& exportedObjects)
{
    Landscape* landscape = FindLandscape(scene);
    if (landscape != nullptr)
    {
        const FilePath& heightmapPath = landscape->GetHeightmapPathname();
        exportedObjects.emplace_back(SceneExporter::OBJECT_HEIGHTMAP, heightmapPath.GetRelativePathname(dataSourceFolder));
    }
}

void CollectTextureDescriptors(Scene* scene, const FilePath& dataSourceFolder, SceneExporter::ExportedObjectCollection& exportedObjects)
{
    TexturesMap sceneTextures;
    SceneHelper::EnumerateSceneTextures(scene, sceneTextures, SceneHelper::TexturesEnumerateMode::INCLUDE_NULL);

    exportedObjects.reserve(exportedObjects.size() + sceneTextures.size());
    for (const auto& scTex : sceneTextures)
    {
        const DAVA::FilePath& path = scTex.first;
        if (path.GetType() == DAVA::FilePath::PATH_IN_MEMORY)
        {
            continue;
        }

        DVASSERT(path.IsEmpty() == false);

        exportedObjects.emplace_back(SceneExporter::OBJECT_TEXTURE, path.GetRelativePathname(dataSourceFolder));
    }
}

} //namespace SceneExporterV2Internal

SceneExporter::~SceneExporter() = default;

void SceneExporter::SetCompressionParams(const DAVA::eGPUFamily gpu, DAVA::TextureConverter::eConvertQuality quality_)
{
    exportForGPU = gpu;
    quality = quality_;
}

void SceneExporter::SetFolders(const FilePath& dataFolder_, const FilePath& dataSourceFolder_)
{
    DVASSERT(dataFolder_.IsDirectoryPathname());
    DVASSERT(dataSourceFolder_.IsDirectoryPathname());

    dataFolder = dataFolder_;
    dataSourceFolder = dataSourceFolder_;
}

void SceneExporter::SetConnectionParams(const SceneExporter::ClientConnectionParams& clientConnectionParams_)
{
    clientConnectionParams = clientConnectionParams_;
}

void SceneExporter::EnableOptimizations(bool enable)
{
    optimizeOnExport = enable;
}

void SceneExporter::ExportSceneFile(const DAVA::FilePath& scenePathname, const DAVA::String& sceneLink)
{
    Logger::Info("Exporting of %s", sceneLink.c_str());

    FilePath outScenePathname = dataFolder + sceneLink;
    FilePath outSceneFolder = outScenePathname.GetDirectory();
    FilePath linksPathname(outSceneFolder + SceneExporterCache::LINKS_NAME);

    SCOPE_EXIT
    { //delete temporary file
        bool exists = FileSystem::Instance()->Exists(linksPathname); //temporary debugging check
        bool deleted = FileSystem::Instance()->DeleteFile(linksPathname);

        DVASSERT(exists == deleted); //temporary debugging and testing check
    };

    ExportedObjectCollection externalLinks;

    AssetCache::CacheItemKey cacheKey;
    if (clientConnectionParams.enabled)
    { //request Scene from cache
        SceneExporterCache::CalculateSceneKey(scenePathname, sceneLink, cacheKey, static_cast<uint32>(optimizeOnExport));

        bool gotFromCache = SceneExporterCache::GetFromCache(outScenePathname.GetDirectory(), cacheKey, clientConnectionParams);
        if (gotFromCache)
        {
            SceneExporterInternal::LoadExportedObjects(linksPathname, externalLinks);
            ExportObjects(externalLinks);
            return;
        }
    }

    { //has no scene in cache or using of cache is disabled. Export scene directly
        ExportSceneFileInternal(scenePathname, externalLinks);
        ExportObjects(externalLinks);
    }

    if (clientConnectionParams.enabled)
    { //place exported scene into cache
        SceneExporterInternal::SaveExportedObjects(linksPathname, externalLinks);

        SceneExporterCache::CachedFiles cachedFiles;
        cachedFiles.push_back(outScenePathname);
        cachedFiles.push_back(linksPathname);
        SceneExporterCache::AddToCache(cachedFiles, cacheKey, clientConnectionParams);
    }
}

void SceneExporter::ExportSceneFileInternal(const FilePath& scenePathname, ExportedObjectCollection& exportedObjects)
{
    //Load scene from *.sc2
    ScopedPtr<Scene> scene(new Scene());
    if (SceneFileV2::ERROR_NO_ERROR == scene->LoadScene(scenePathname))
    {
        ExportScene(scene, scenePathname, exportedObjects);
    }
    else
    {
        Logger::Error("[SceneExporterV2::%s] Can't open file %s", __FUNCTION__, scenePathname.GetAbsolutePathname().c_str());
    }

    RenderObjectsFlusher::Flush();
}

void SceneExporter::ExportTextureFile(const FilePath& descriptorPathname, const String& descriptorLink)
{
    std::unique_ptr<TextureDescriptor> descriptor(TextureDescriptor::CreateFromFile(descriptorPathname));
    if (!descriptor)
    {
        Logger::Error("Can't create descriptor for pathname %s", descriptorPathname.GetAbsolutePathname().c_str());
        return;
    }

    descriptor->exportedAsGpuFamily = exportForGPU;
    descriptor->format = descriptor->GetPixelFormatForGPU(exportForGPU);
    if (GPUFamilyDescriptor::IsGPUForDevice(exportForGPU))
    {
        if (descriptor->format == FORMAT_INVALID)
        {
            Logger::Error("Not selected export format for pathname %s", descriptorPathname.GetAbsolutePathname().c_str());
            return;
        }

        FilePath compressedTexturePathname = CompressTexture(*descriptor);
        CopyFile(compressedTexturePathname);
    }
    else
    {
        CopySourceTexture(*descriptor);
    }

    FilePath exportedDescriptorPath = dataFolder + descriptorLink;
    descriptor->Export(exportedDescriptorPath);
}

void SceneExporter::ExportHeightmapFile(const DAVA::FilePath& heightmapPathname, const DAVA::String& heightmapLink)
{
    CopyFile(heightmapPathname, heightmapLink);
}

DAVA::FilePath SceneExporter::CompressTexture(DAVA::TextureDescriptor& descriptor) const
{
    DVASSERT(GPUFamilyDescriptor::IsGPUForDevice(exportForGPU));

    FilePath compressedTexureName = descriptor.CreatePathnameForGPU(exportForGPU);

    const bool needToConvert = !descriptor.IsCompressedTextureActual(exportForGPU);
    if (needToConvert)
    {
        Logger::Warning("Need recompress texture: %s", descriptor.GetSourceTexturePathname().GetAbsolutePathname().c_str());
        return TextureConverter::ConvertTexture(descriptor, exportForGPU, true, quality);
    }

    return compressedTexureName;
}

void SceneExporter::CopySourceTexture(DAVA::TextureDescriptor& descriptor) const
{
    if (descriptor.IsCubeMap())
    {
        Vector<FilePath> faceNames;
        descriptor.GetFacePathnames(faceNames);
        for (const auto& faceName : faceNames)
        {
            if (faceName.IsEmpty())
                continue;

            CopyFile(faceName);
        }
    }
    else
    {
        CopyFile(descriptor.GetSourceTexturePathname());
    }
}

bool SceneExporter::CopyFile(const DAVA::FilePath& filePath) const
{
    String workingPathname = filePath.GetRelativePathname(dataSourceFolder);
    return CopyFile(filePath, workingPathname);
}

bool SceneExporter::CopyFile(const DAVA::FilePath& filePath, const DAVA::String& fileLink) const
{
    FilePath newFilePath = dataFolder + fileLink;

    bool retCopy = FileSystem::Instance()->CopyFile(filePath, newFilePath, true);
    if (!retCopy)
    {
        Logger::Error("Can't copy %s to %s",
                      fileLink.c_str(),
                      newFilePath.GetAbsolutePathname().c_str());
    }

    return retCopy;
}

bool SceneExporter::ExportScene(Scene* scene, const FilePath& scenePathname, ExportedObjectCollection& exportedObjects)
{
    String relativeSceneFilename = scenePathname.GetRelativePathname(dataSourceFolder);
    FilePath outScenePathname = dataFolder + relativeSceneFilename;

    SceneExporterInternal::PrepareSceneToExport(scene, optimizeOnExport);

    SceneExporterInternal::CollectHeightmapPathname(scene, dataSourceFolder, exportedObjects); //must be first
    SceneExporterInternal::CollectTextureDescriptors(scene, dataSourceFolder, exportedObjects);

    // save scene to new place
    FilePath tempSceneName = FilePath::CreateWithNewExtension(scenePathname, ".exported.sc2");
    scene->SaveScene(tempSceneName, optimizeOnExport);

    bool moved = FileSystem::Instance()->MoveFile(tempSceneName, outScenePathname, true);
    if (!moved)
    {
        Logger::Error("Can't move file %s into %s", tempSceneName.GetAbsolutePathname().c_str(), outScenePathname.GetAbsolutePathname().c_str());
        FileSystem::Instance()->DeleteFile(tempSceneName);
        return false;
    }

    return true;
}

void SceneExporter::ExportObjects(const ExportedObjectCollection& exportedObjects)
{
    UnorderedSet<String> folders;
    folders.reserve(exportedObjects.size());

    String inFolderString = dataSourceFolder.GetAbsolutePathname();
    String outFolderString = dataFolder.GetAbsolutePathname();

    //enumerate target folders for exported objects
    for (const auto& object : exportedObjects)
    {
        const String& link = object.relativePathname;

        const String::size_type slashpos = link.rfind(String("/"));
        if (slashpos != String::npos)
        {
            folders.insert(link.substr(0, slashpos + 1));
        }
    }

    //Create folders in Data Folder to copy objects
    for (const auto& folder : folders)
    {
        FileSystem::Instance()->CreateDirectory(outFolderString + folder, true);
    }

    using ExporterFunction = DAVA::Function<void(const DAVA::FilePath&, const DAVA::String&)>;
    Array<ExporterFunction, OBJECT_COUNT> exporters =
    { { DAVA::MakeFunction(this, &SceneExporter::ExportSceneFile),
        DAVA::MakeFunction(this, &SceneExporter::ExportTextureFile),
        DAVA::MakeFunction(this, &SceneExporter::ExportHeightmapFile) } };

    for (const auto& object : exportedObjects)
    {
        if (object.type != OBJECT_NONE && object.type < OBJECT_COUNT)
        {
            FilePath path(inFolderString + object.relativePathname);
            exporters[object.type](path, object.relativePathname);
        }
        else
        {
            Logger::Error("Found wrong path: %s", object.relativePathname.c_str());
            continue; //need continue exporting of resources in any case.
        }
    }
}
