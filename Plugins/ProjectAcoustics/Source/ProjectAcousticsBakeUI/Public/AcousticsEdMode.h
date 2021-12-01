// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//   Created from SFoliageEdit class code

#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"
#include "AcousticsMaterialsTab.h"
#include "AcousticsBakeTab.h"
#include "Misc/ConfigCacheIni.h"

const FName c_AcousticsGeometryTag = "AcousticsGeometry";
const FName c_AcousticsNavigationTag = "AcousticsNavigation";
const FString c_ConfigSectionMaterials = TEXT("Materials");
const FString c_PluginName = TEXT("ProjectAcoustics");

DECLARE_LOG_CATEGORY_EXTERN(LogAcoustics, Log, All);

enum AcousticsActiveTab
{
    ObjectTag,
    Materials,
    Probes,
    Bake
};

struct AcousticsObjectsTabSettings
{
    bool IsStaticMeshChecked;
    bool IsNavMeshChecked;
    bool IsLandscapeChecked;
    bool IsAcousticsRadioButtonChecked;
    bool IsNavigationRadioButtonChecked;
};

struct AcousticsUISettings
{
    AcousticsActiveTab CurrentTab;
    AcousticsObjectsTabSettings ObjectsTabSettings;
    // TODO more here
};

class FAcousticsEdMode : public FEdMode
{
public:
    const static FEditorModeID EM_AcousticsEdModeId;

public:
    FAcousticsEdMode();
    virtual ~FAcousticsEdMode();

    // FEdMode interface
    virtual void Enter() override;
    virtual void Exit() override;
    // virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
    // virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
    // virtual void ActorSelectionChangeNotify() override;
    bool UsesToolkits() const override;
    // End of FEdMode interface

    void SetMaterialsTab(TSharedPtr<SAcousticsMaterialsTab>&& materialsTab)
    {
        m_materialsTab = materialsTab;
    }

    TSharedPtr<SAcousticsMaterialsTab> GetMaterialsTab()
    {
        return m_materialsTab;
    }

    void SetBakeTab(TSharedPtr<SAcousticsBakeTab>&& bakeTab)
    {
        m_BakeTab = bakeTab;
    }

    TSharedPtr<FUICommandList> UICommandList;
    AcousticsUISettings AcousticsUISettings;

    void OnClickObjectTab()
    {
        m_materialsTab->PublishMaterialLibrary();
        AcousticsUISettings.CurrentTab = AcousticsActiveTab::ObjectTag;
    }

    void OnClickMaterialsTab()
    {
        m_materialsTab->UpdateUEMaterials();
        AcousticsUISettings.CurrentTab = AcousticsActiveTab::Materials;
    }

    void OnClickProbesTab()
    {
        m_materialsTab->PublishMaterialLibrary();
        AcousticsUISettings.CurrentTab = AcousticsActiveTab::Probes;
    }

    void OnClickBakeTab()
    {
        m_materialsTab->PublishMaterialLibrary();
        m_BakeTab->Refresh();
        AcousticsUISettings.CurrentTab = AcousticsActiveTab::Bake;
    }

    // Object Tab Helpers
    void SelectObjects();
    bool TagGeometry(bool tag);
    bool TagNavigation(bool tag);

    // Configuration Helper
    bool GetConfigFile(FConfigFile** configFile, FString& configFilePath);

private:
    void BindCommands();

private:
    TSharedPtr<SAcousticsMaterialsTab> m_materialsTab;
    TSharedPtr<SAcousticsBakeTab> m_BakeTab;

    FConfigFile m_ConfigFile;
    FString m_ConfigFilePath;
};
