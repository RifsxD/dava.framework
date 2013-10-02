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



#include "BeastAction.h"
#include "Classes/Qt/Scene/SceneEditor2.h"
#include "Classes/Qt/Main/mainwindow.h"
#include "Classes/BeastProxy.h"
#include "Classes/LightmapsPacker.h"
#include "Classes/SceneEditor/EditorSettings.h"

#include "DAVAEngine.h"

using namespace DAVA;

#if defined (__DAVAENGINE_BEAST__)

//Beast
BeastAction::BeastAction(SceneEditor2 *scene)
	: CommandAction(CMDID_BEAST, "Beast")
	, workingScene(scene)
{
	beastManager = BeastProxy::Instance()->CreateManager();
}

BeastAction::~BeastAction()
{
	BeastProxy::Instance()->SafeDeleteManager(&beastManager);
}

void BeastAction::Redo()
{
	Start();

	while( Process() == false )
	{
		bool canceled = QtMainWindow::Instance()->BeastWaitCanceled();
		if(canceled) 
		{
			BeastProxy::Instance()->Cancel(beastManager);
		}

		uint64 deltaTime = SystemTimer::Instance()->AbsoluteMS() - startTime;

		QtMainWindow::Instance()->BeastWaitSetMessage(Format("Beasting %d sec", deltaTime/1000));
		Sleep(15);
	}

	Finish();
}

void BeastAction::Start()
{
	startTime = SystemTimer::Instance()->AbsoluteMS();

	FilePath path = GetLightmapDirectoryPath();
	FileSystem::Instance()->CreateDirectory(path, false);

	BeastProxy::Instance()->SetLightmapsDirectory(beastManager, path);
	BeastProxy::Instance()->Run(beastManager, workingScene);
}

bool BeastAction::Process()
{
	BeastProxy::Instance()->Update(beastManager);
	return BeastProxy::Instance()->IsJobDone(beastManager);
}

void BeastAction::Finish()
{
	PackLightmaps();

	Landscape *land = FindLandscape(workingScene);
	if(land)
	{
		FilePath textureName = land->GetTextureName(DAVA::Landscape::TEXTURE_COLOR);
		textureName.ReplaceFilename("temp_beast.png");

		FileSystem::Instance()->DeleteFile(textureName);
	}
}


void BeastAction::PackLightmaps()
{
	FilePath inputDir = GetLightmapDirectoryPath();
	FilePath outputDir = FilePath::CreateWithNewExtension(workingScene->GetScenePath(),  ".sc2_lightmaps/");

	FileSystem::Instance()->MoveFile(inputDir+"landscape.png", "test_landscape.png", true);

	LightmapsPacker packer;
	packer.SetInputDir(inputDir);

	packer.SetOutputDir(outputDir);
	packer.PackLightmaps();
	packer.CreateDescriptors();
	packer.ParseSpriteDescriptors();

	BeastProxy::Instance()->UpdateAtlas(beastManager, packer.GetAtlasingData());

	FileSystem::Instance()->MoveFile("test_landscape.png", outputDir+"landscape.png", true);
}

DAVA::FilePath BeastAction::GetLightmapDirectoryPath()
{
	return EditorSettings::Instance()->GetProjectPath() + "DataSource/lightmaps_temp/";
}


#endif //#if defined (__DAVAENGINE_BEAST__)


