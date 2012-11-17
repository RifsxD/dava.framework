#include "SceneExporter.h"
#include "SceneValidator.h"

#include "PVRConverter.h"

#include "Render/TextureDescriptor.h"


using namespace DAVA;

SceneExporter::SceneExporter()
{
    dataFolder = String("");
    dataSourceFolder = String(""); 
    workingFolder = String("");
    
    SetExportingFormat(String("png"));
}

SceneExporter::~SceneExporter()
{
    ReleaseTextures();
}


void SceneExporter::CleanFolder(const String &folderPathname, Set<String> &errorLog)
{
    bool ret = FileSystem::Instance()->DeleteDirectory(folderPathname);
    if(!ret)
    {
        bool folderExists = FileSystem::Instance()->IsDirectory(folderPathname);
        if(folderExists)
        {
            errorLog.insert(String(Format("[CleanFolder] ret = %d, folder = %s", ret, folderPathname.c_str())));
        }
    }
}

void SceneExporter::SetInFolder(const String &folderPathname)
{
    dataSourceFolder = NormalizeFolderPath(folderPathname);
}

void SceneExporter::SetOutFolder(const String &folderPathname)
{
    dataFolder = NormalizeFolderPath(folderPathname);
}

void SceneExporter::SetExportingFormat(const String &newFormat)
{
    String format = newFormat;
    if(!format.empty() && format.at(0) != '.')
    {
        format = "." + format;
    }
    
    exportFormat = NOT_FILE;
    if(0 == CompareCaseInsensitive(format, ".png"))
    {
        exportFormat = PNG_FILE;
    }
    else if(0 == CompareCaseInsensitive(format, ".pvr"))
    {
        exportFormat = PVR_FILE;
    }
    else if(0 == CompareCaseInsensitive(format, ".dxt"))
    {
        exportFormat = DXT_FILE;
    }
}

void SceneExporter::ExportFile(const String &fileName, Set<String> &errorLog)
{
    //Load scene with *.sc2
    Scene *scene = new Scene();
    SceneNode *rootNode = scene->GetRootNode(dataSourceFolder + fileName);
    if(rootNode)
    {
        int32 count = rootNode->GetChildrenCount();
		Vector<SceneNode*> tempV;
		tempV.reserve((count));
        for(int32 i = 0; i < count; ++i)
        {
			tempV.push_back(rootNode->GetChild(i));
        }
		for(int32 i = 0; i < count; ++i)
		{
			scene->AddNode(tempV[i]);
		}
    }
    
    ExportScene(scene, fileName, errorLog);

    SafeRelease(scene);
}

void SceneExporter::ExportScene(Scene *scene, const String &fileName, Set<String> &errorLog)
{
    DVASSERT(0 == texturesForExport.size())
    DVASSERT(0 == exportedTextures.size())
    
    //Create destination folder
    String normalizedFileName = FileSystem::Instance()->GetCanonicalPath(fileName);

    
    String workingFile;
    FileSystem::SplitPath(normalizedFileName, workingFolder, workingFile);
    FileSystem::Instance()->CreateDirectory(dataFolder + workingFolder, true); 
    
    //Export scene data
    RemoveEditorNodes(scene);

    String oldPath = SceneValidator::Instance()->SetPathForChecking(dataSourceFolder);
    SceneValidator::Instance()->ValidateScene(scene, errorLog);
	//SceneValidator::Instance()->ValidateScales(scene, errorLog);

    EnumerateTextures(scene, errorLog);

    ExportTextures(scene, errorLog);
    ExportLandscape(scene, errorLog);
    
    ReleaseTextures();
    
    //save scene to new place
    SceneFileV2 * outFile = new SceneFileV2();
    outFile->EnableSaveForGame(true);
    outFile->EnableDebugLog(false);
    outFile->SaveScene(dataFolder + fileName, scene);
    SafeRelease(outFile);
    
    SceneValidator::Instance()->SetPathForChecking(oldPath);
}

void SceneExporter::RemoveEditorNodes(DAVA::SceneNode *rootNode)
{
    //Remove scene nodes
    Vector<SceneNode *> scenenodes;
    rootNode->GetChildNodes(scenenodes);
        
    //remove nodes from hierarhy
    Vector<SceneNode *>::reverse_iterator endItDeletion = scenenodes.rend();
    for (Vector<SceneNode *>::reverse_iterator it = scenenodes.rbegin(); it != endItDeletion; ++it)
    {
        SceneNode * node = *it;
		String::size_type pos = node->GetName().find(String("editor."));
        if(String::npos != pos)
        {
            node->GetParent()->RemoveNode(node);
        }
		else
		{
			LightNode * light = dynamic_cast<LightNode*>(node);
			if(light)
			{
				bool isDynamic = light->GetCustomProperties()->GetBool("editor.dynamiclight.enable", true);
				if(!isDynamic)
				{
					node->GetParent()->RemoveNode(node);
				}
			}
		}
    }
}


void SceneExporter::EnumerateTextures(DAVA::Scene *scene, Set<String> &errorLog)
{
    //materials
    Vector<Material*> materials;
    scene->GetDataNodes(materials);
    for(int32 m = 0; m < (int32)materials.size(); ++m)
    {
        for(int32 t = 0; t < Material::TEXTURE_COUNT; ++t)
        {
            Texture *tex = materials[m]->GetTexture((Material::eTextureLevel)t);
            CollectTextureForExport(tex);
        }
    }

    //landscapes
    Vector<LandscapeNode *> landscapes;
    scene->GetChildNodes(landscapes);
    for(int32 l = 0; l < (int32)landscapes.size(); ++l)
    {
        for(int32 t = 0; t < LandscapeNode::TEXTURE_COUNT; t++)
        {
            Texture *tex = landscapes[l]->GetTexture((LandscapeNode::eTextureLevel)t);
            CollectTextureForExport(tex);
        }
    }
    
    //lightmaps
    Vector<MeshInstanceNode *> meshInstances;
    scene->GetChildNodes(meshInstances);
    for(int32 m = 0; m < (int32)meshInstances.size(); ++m)
    {
        for (int32 li = 0; li < meshInstances[m]->GetLightmapCount(); ++li)
        {
            MeshInstanceNode::LightmapData * ld = meshInstances[m]->GetLightmapDataForIndex(li);
            if (ld)
            {
                CollectTextureForExport(ld->lightmap);
            }
        }
    }
}

void SceneExporter::ExportTextures(DAVA::Scene *scene, Set<String> &errorLog)
{
    Set<String>::const_iterator endIt = texturesForExport.end();
    Set<String>::iterator it = texturesForExport.begin();
    for( ; it != endIt; ++it)
    {
        exportedTextures[*it] = ExportTexture( *it, errorLog);
    }
}

void SceneExporter::ReleaseTextures()
{
    texturesForExport.clear();
    exportedTextures.clear();
}

void SceneExporter::ExportFolder(const String &folderName, Set<String> &errorLog)
{
    String folderPathname = dataSourceFolder + NormalizeFolderPath(folderName);
	FileList * fileList = new FileList(folderPathname);
    for (int32 i = 0; i < fileList->GetCount(); ++i)
	{
        String pathname = fileList->GetPathname(i);
		if(fileList->IsDirectory(i))
		{
            String curFolderName = fileList->GetFilename(i);
            if((String(".") != curFolderName) && (String("..") != curFolderName))
            {
                String workingPathname = RemoveFolderFromPath(pathname, dataSourceFolder);
                ExportFolder(workingPathname, errorLog);
            }
        }
        else 
        {
            String extension = FileSystem::Instance()->GetExtension(pathname);
            if(String(".sc2") == extension)
            {
                String workingPathname = RemoveFolderFromPath(pathname, dataSourceFolder);
                ExportFile(workingPathname, errorLog);
            }
        }
    }
    
    SafeRelease(fileList);
}

String SceneExporter::NormalizeFolderPath(const String &pathname)
{
    String normalizedPathname = FileSystem::Instance()->GetCanonicalPath(pathname);

    int32 lastPos = normalizedPathname.length() - 1;
    if((0 <= lastPos) && ('/' != normalizedPathname.at(lastPos)))
    {
        normalizedPathname += "/";
    }
    
    return normalizedPathname;
}


void SceneExporter::ExportLandscape(Scene *scene, Set<String> &errorLog)
{
    Logger::Debug("[ExportLandscape]");

    Vector<LandscapeNode *> landscapes;
    scene->GetChildNodes(landscapes);
    
    if(0 < landscapes.size())
    {
        if(1 < landscapes.size())
        {
            errorLog.insert(String("There are more than one landscapes at level."));
        }
        
        LandscapeNode *landscape = landscapes[0];
        ExportFileDirectly(landscape->GetHeightmapPathname(), errorLog);
        ExportLandscapeFullTiledTexture(landscape, errorLog);
    }
}

void SceneExporter::ExportLandscapeFullTiledTexture(LandscapeNode *landscape, Set<String> &errorLog)
{
    String textureName = landscape->GetTextureName(LandscapeNode::TEXTURE_TILE_FULL);
    if(textureName.empty())
    {
        String colorTextureMame = landscape->GetTextureName(LandscapeNode::TEXTURE_COLOR);
        String filename, pathname;
        FileSystem::Instance()->SplitPath(colorTextureMame, pathname, filename);
        
        String fullTiledPathname = pathname + FileSystem::Instance()->ReplaceExtension(filename, ".thumbnail.png");
        String workingPathname = RemoveFolderFromPath(fullTiledPathname, dataSourceFolder);
        PrepareFolderForCopy(workingPathname, errorLog);

        Texture *fullTiledTexture = Texture::GetPinkPlaceholder();
        Image *image = fullTiledTexture->CreateImageFromMemory();
        if(image)
        {
            ImageLoader::Save(image, dataFolder + workingPathname);
            SafeRelease(image);
            
            TextureDescriptor *descriptor = new TextureDescriptor();
            if(exportFormat == PVR_FILE)
            {
                descriptor->pvrCompression.format = FORMAT_PVR4;
            }
            
            String descriptorPathname = TextureDescriptor::GetDescriptorPathname(workingPathname);
            descriptor->Save(dataFolder + descriptorPathname);
            SafeRelease(descriptor);
            
            landscape->SetTexture(LandscapeNode::TEXTURE_TILE_FULL, workingPathname);

            errorLog.insert(String(Format("Full tiled texture is autogenerated png-image.", workingPathname.c_str())));
        }
        else
        {
            errorLog.insert(String(Format("Can't create image for fullTiled Texture for file %s", workingPathname.c_str())));
            landscape->SetTextureName(LandscapeNode::TEXTURE_TILE_FULL, String(""));
        }
        
        landscape->SetTextureName(LandscapeNode::TEXTURE_TILE_FULL, dataSourceFolder + workingPathname);
    }
}


bool SceneExporter::ExportFileDirectly(const String &filePathname, Set<String> &errorLog)
{
    //    Logger::Debug("[ExportFileDirectly] %s", filePathname.c_str());

    String workingPathname = RemoveFolderFromPath(filePathname, dataSourceFolder);
    PrepareFolderForCopy(workingPathname, errorLog);
    
    bool retCopy = FileSystem::Instance()->CopyFile(dataSourceFolder + workingPathname, dataFolder + workingPathname);
    if(!retCopy)
    {
        errorLog.insert(String(Format("Can't copy %s from %s to %s", workingPathname.c_str(), dataSourceFolder.c_str(), dataFolder.c_str())));
    }
    
    return retCopy;
}

void SceneExporter::PrepareFolderForCopy(const String &filePathname, Set<String> &errorLog)
{
    String folder, file;
    FileSystem::SplitPath(filePathname, folder, file);
    
    String newFolderPath = dataFolder + folder;
    if(!FileSystem::Instance()->IsDirectory(newFolderPath))
    {
        FileSystem::eCreateDirectoryResult retCreate = FileSystem::Instance()->CreateDirectory(newFolderPath, true);
        if(FileSystem::DIRECTORY_CANT_CREATE == retCreate)
        {
            errorLog.insert(String(Format("Can't create folder %s", newFolderPath.c_str())));
        }
    }
    
    FileSystem::Instance()->DeleteFile(dataFolder + filePathname);
}



bool SceneExporter::ExportTexture(const String &texturePathname, Set<String> &errorLog)
{
    //TODO: Create correct export
    
    ExportTextureDescriptor(texturePathname, errorLog);
    CompressTextureIfNeed(texturePathname, errorLog);
    
    bool ret = ExportFileDirectly(GetExportedTextureName(texturePathname), errorLog);
//    if(!ret)
//    {
//        //TODO: blen textures
//        RenderManager::Instance()->LockNonMain();
//
//        Texture *tex = Texture::CreateFromFile(texturePathname);
//        if(tex)
//        {
//            Sprite *fbo = Sprite::CreateAsRenderTarget((float32)tex->width, (float32)tex->height, FORMAT_RGBA8888);
//            Sprite *texSprite = Sprite::CreateFromTexture(tex, 0, 0, (float32)tex->width, (float32)tex->height);
//            if(fbo && texSprite)
//            {
//                
//                RenderManager::Instance()->SetRenderTarget(fbo);
//                texSprite->SetPosition(0.f, 0.f);
//                texSprite->Draw();
//                
//                RenderManager::Instance()->SetColor(Color(1.0f, 0.0f, 1.0f, 0.5f));
//                RenderHelper::Instance()->FillRect(Rect(0.0f, 0.0f, (float32)tex->width, (float32)tex->height));
//
//                RenderManager::Instance()->RestoreRenderTarget();
//                
//                //Save new texture
//                String workingPathname = RemoveFolderFromPath(texturePathname, dataSourceFolder);
//                PrepareFolderForCopy(workingPathname, errorLog);
//                exportedPathname = dataFolder + workingPathname;
//                
//                Image *image = fbo->GetTexture()->CreateImageFromMemory();
//                if(image)
//                {
//                    ImageLoader::Save(image, exportedPathname);
//                }
//                else 
//                {
//                    errorLog.insert(String(Format("Can't create image from fbo for file %s", texturePathname.c_str())));
//                }
//                
//                SafeRelease(image);
//            }
//            else 
//            {
//                errorLog.insert(String(Format("Can't create FBO for file %s", texturePathname.c_str())));
//            }
//            
//            SafeRelease(texSprite);
//            SafeRelease(fbo);
//            SafeRelease(tex);
//        }
//        else 
//        {
//            errorLog.insert(String(Format("Can't create texture for file %s", texturePathname.c_str())));
//        }
//        RenderManager::Instance()->UnlockNonMain();
//        
//    }
    
    return ret;
}

void SceneExporter::ExportTextureDescriptor(const String &texturePathname, Set<String> &errorLog)
{
    TextureDescriptor *descriptor = TextureDescriptor::CreateFromFile(texturePathname);
    DVASSERT(descriptor && "Decriptors mast be created for all textures");
    
    String workingPathname = RemoveFolderFromPath(descriptor->pathname, dataSourceFolder);
    PrepareFolderForCopy(workingPathname, errorLog);
    
#if defined TEXTURE_SPLICING_ENABLED
    descriptor->ExportAndSlice(dataFolder + workingPathname, GetExportedTextureName(texturePathname));
#else //#if defined TEXTURE_SPLICING_ENABLED
    descriptor->Export(dataFolder + workingPathname);
#endif //#if defined TEXTURE_SPLICING_ENABLED

    SafeRelease(descriptor);
}


void SceneExporter::CompressTextureIfNeed(const String &texturePathname, Set<String> &errorLog)
{
    String modificationDate = File::GetModificationDate(GetExportedTextureName(texturePathname));
    
    String sourceTexturePathname = FileSystem::Instance()->ReplaceExtension(texturePathname, ".png");
    
    if(PNG_FILE != exportFormat)
    {
        bool needToConvert = SceneValidator::IsTextureChanged(sourceTexturePathname, exportFormat);
        if(needToConvert || modificationDate.empty())
        {
            //TODO: convert to pvr/dxt
            //TODO: do we need to convert to pvr if needToConvert is false, but *.pvr file isn't at filesystem
            
            TextureDescriptor *descriptor = TextureDescriptor::CreateFromFile(texturePathname);
            DVASSERT(descriptor && "Decriptors mast be created for all textures");
            if(exportFormat == PVR_FILE)
            {
                PVRConverter::Instance()->ConvertPngToPvr(sourceTexturePathname, *descriptor);
            }
            
            SafeRelease(descriptor);
        }
    }
}

String SceneExporter::RemoveFolderFromPath(const String &pathname, const String &folderPathname)
{
    String workingPathname = pathname;
    String::size_type pos = workingPathname.find(folderPathname);
    if(String::npos != pos)
    {
        workingPathname = workingPathname.replace(pos, folderPathname.length(), "");
    }

    return workingPathname;
}

String SceneExporter::GetExportedTextureName(const String &pathname)
{
    String exportedPathname = String("");
    
    switch (exportFormat)
    {
        case PNG_FILE:
            exportedPathname = FileSystem::Instance()->ReplaceExtension(pathname, String(".png"));
            break;

        case PVR_FILE:
            exportedPathname = FileSystem::Instance()->ReplaceExtension(pathname, String(".pvr"));
            break;

        case DXT_FILE:
            exportedPathname = FileSystem::Instance()->ReplaceExtension(pathname, String(".dxt"));
            break;

        default:
            break;
    }
    
    return exportedPathname;
}

void SceneExporter::CollectTextureForExport(Texture *texture)
{
    if(texture && !texture->isRenderTarget)
    {
        String exportedPathname = texture->GetPathname();
        String::size_type textTexturePos = texture->GetPathname().find("Text texture");
        if(!exportedPathname.empty() && (String::npos == textTexturePos))
        {
            texturesForExport.insert(exportedPathname);
        }
    }
}

