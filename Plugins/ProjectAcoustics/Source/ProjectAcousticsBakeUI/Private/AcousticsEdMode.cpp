// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//   Created from SFoliageEdit class code

#include "AcousticsEdMode.h"
#include "Runtime/Launch/Resources/Version.h"
#include "AcousticsEdModeToolkit.h"
#include "AcousticsEditActions.h"
#include "Toolkits/ToolkitManager.h"
#include "EditorModeManager.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Landscape.h"
#include "Engine/StaticMeshActor.h"
#include "Interfaces/IPluginManager.h"
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 20
#include "AI/Navigation/RecastNavMesh.h"
#else
#include "Navmesh/RecastNavMesh.h"
#endif

const FEditorModeID FAcousticsEdMode::EM_AcousticsEdModeId = TEXT("EM_AcousticsEdMode");

DEFINE_LOG_CATEGORY(LogAcoustics)

FAcousticsEdMode::FAcousticsEdMode()
{
    FAcousticsEditCommands::Register();

    UICommandList = MakeShareable(new FUICommandList);
    BindCommands();

    AcousticsUISettings.CurrentTab = AcousticsActiveTab::ObjectTag;
    AcousticsUISettings.ObjectsTabSettings.IsNavMeshChecked = false;
    AcousticsUISettings.ObjectsTabSettings.IsLandscapeChecked = false;
    AcousticsUISettings.ObjectsTabSettings.IsStaticMeshChecked = false;
    AcousticsUISettings.ObjectsTabSettings.IsAcousticsRadioButtonChecked = true;
    AcousticsUISettings.ObjectsTabSettings.IsNavigationRadioButtonChecked = false;
}

FAcousticsEdMode::~FAcousticsEdMode()
{
}

void FAcousticsEdMode::Enter()
{
    FEdMode::Enter();

    if (!Toolkit.IsValid() && UsesToolkits())
    {
        Toolkit = MakeShareable(new FAcousticsEdModeToolkit);
        Toolkit->Init(Owner->GetToolkitHost());
    }
}

void FAcousticsEdMode::Exit()
{
    if (Toolkit.IsValid())
    {
        FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
        Toolkit.Reset();
    }

    // Call base Exit method to ensure proper cleanup
    FEdMode::Exit();
}

bool FAcousticsEdMode::UsesToolkits() const
{
    return true;
}

void FAcousticsEdMode::BindCommands()
{
    const FAcousticsEditCommands& Commands = FAcousticsEditCommands::Get();

    UICommandList->MapAction(
        Commands.SetObjectTag,
        FExecuteAction::CreateRaw(this, &FAcousticsEdMode::OnClickObjectTab),
        FCanExecuteAction(),
        FIsActionChecked::CreateLambda(
            [=] { return AcousticsUISettings.CurrentTab == AcousticsActiveTab::ObjectTag; }));

    UICommandList->MapAction(
        Commands.SetMaterials,
        FExecuteAction::CreateRaw(this, &FAcousticsEdMode::OnClickMaterialsTab),
        FCanExecuteAction(),
        FIsActionChecked::CreateLambda(
            [=] { return AcousticsUISettings.CurrentTab == AcousticsActiveTab::Materials; }));

    UICommandList->MapAction(
        Commands.SetProbes,
        FExecuteAction::CreateRaw(this, &FAcousticsEdMode::OnClickProbesTab),
        FCanExecuteAction(),
        FIsActionChecked::CreateLambda([=] { return AcousticsUISettings.CurrentTab == AcousticsActiveTab::Probes; }));

    UICommandList->MapAction(
        Commands.SetBake,
        FExecuteAction::CreateRaw(this, &FAcousticsEdMode::OnClickBakeTab),
        FCanExecuteAction(),
        FIsActionChecked::CreateLambda([=] { return AcousticsUISettings.CurrentTab == AcousticsActiveTab::Bake; }));
}

void FAcousticsEdMode::SelectObjects()
{
    GEditor->SelectNone(true, true, false);

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ULevel* CurrentLevel = World->GetCurrentLevel();
    const int32 NumLevels = World->GetNumLevels();

    for (int32 LevelIdx = 0; LevelIdx < NumLevels; ++LevelIdx)
    {
        ULevel* Level = World->GetLevel(LevelIdx);
        if (Level && Level->bIsVisible)
        {
            // Add all objects that fit a filter to some internal list
            auto actors = Level->Actors;
            for (const auto& actor : actors)
            {
                // Sometimes actors don't exist for some reason. Just skip them
                if (actor == nullptr)
                {
                    continue;
                }
                bool shouldSelectActor = false;
                if (AcousticsUISettings.ObjectsTabSettings.IsStaticMeshChecked)
                {
                    if (actor->IsA<AStaticMeshActor>() && actor->IsRootComponentStatic())
                    {
                        shouldSelectActor = true;
                    }
                }
                if (AcousticsUISettings.ObjectsTabSettings.IsLandscapeChecked)
                {
                    if (actor->IsA<ALandscapeProxy>())
                    {
                        shouldSelectActor = true;
                    }
                }
                if (AcousticsUISettings.ObjectsTabSettings.IsNavMeshChecked)
                {
                    if (actor->IsA<ARecastNavMesh>())
                    {
                        shouldSelectActor = true;
                    }
                }

                if (shouldSelectActor)
                {
                    GEditor->SelectActor(actor, true, false, true, false);
                }
            }
        }
    }

    GEditor->NoteSelectionChange();
}

// Returns true if all tags were able to be set, false if there was a problem with one or more tags
bool FAcousticsEdMode::TagGeometry(bool tag)
{
    bool allTagsSet = true;
    for (TActorIterator<AActor> ActorItr(GEditor->GetEditorWorldContext().World()); ActorItr; ++ActorItr)
    {
        AActor* actor = *ActorItr;
        if (actor->IsSelected())
        {
            // Always remove any previously set tags. This prevents multiple-tagging
            actor->Tags.Remove(c_AcousticsGeometryTag);
            if (tag)
            {
                if (actor->IsA(ARecastNavMesh::StaticClass()))
                {
                    allTagsSet = false;
                    UE_LOG(
                        LogAcoustics,
                        Error,
                        TEXT("Attempted to add %s tag to %s, which is a Nav Mesh. This is not "
                             "supported. Skipping tag."),
                        *c_AcousticsGeometryTag.ToString(),
                        *(actor->GetName()));
                    continue;
                }
                actor->Tags.Add(c_AcousticsGeometryTag);
            }
        }
    }

    return allTagsSet;
}

// Returns true if all tags were able to be set, false if there was a problem with one or more tags
bool FAcousticsEdMode::TagNavigation(bool tag)
{
    bool allTagsSet = true;
    for (TActorIterator<AActor> ActorItr(GEditor->GetEditorWorldContext().World()); ActorItr; ++ActorItr)
    {
        AActor* actor = *ActorItr;
        if (actor->IsSelected())
        {
            // Always remove any previously set tags. This prevents multiple-tagging
            actor->Tags.Remove(c_AcousticsNavigationTag);
            if (tag)
            {
                actor->Tags.Add(c_AcousticsNavigationTag);
            }
        }
    }

    return allTagsSet;
}

// Gets the FConfigFile and FString assocaited with the ProjectAcoustics config file
// The config file stores material properties
// On success, returns true and both parameters are populated
// On failure, returns false and neither parameter is changed
bool FAcousticsEdMode::GetConfigFile(FConfigFile** configFile, FString& configFilePath)
{
    // If the config file is not initialized, deserialize it from the plugin's config directly
    // If it has already been read into memory, just return it below
    if (!m_ConfigFile.Name.IsValid() || m_ConfigFilePath.IsEmpty())
    {
        static TSharedPtr<IPlugin> ProjectAcousticsPlugin = IPluginManager::Get().FindPlugin(c_PluginName);
        if (!ProjectAcousticsPlugin.IsValid())
        {
            return false;
        }
        m_ConfigFilePath = GConfig->GetDestIniFilename(
            *c_PluginName, nullptr, *FPaths::Combine(ProjectAcousticsPlugin->GetBaseDir(), TEXT("Config/")));
        m_ConfigFile.Read(m_ConfigFilePath);
    }
    *configFile = &m_ConfigFile;
    configFilePath = m_ConfigFilePath;
    return true;
}