// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "AcousticsProbesTab.h"
#include "AcousticsProbeVolume.h"
#include "AcousticsPinnedProbe.h"
#include "AcousticsDynamicOpening.h"
#include "SAcousticsEdit.h"
#include "AcousticsSharedState.h"
#include "AcousticsEdMode.h"
#include "CollisionGeometryToAcousticMeshConverter.h"
#include "AcousticsSimulationConfiguration.h"
#include "MathUtils.h"
#include "Fonts/SlateFontInfo.h"
#include "Modules/ModuleManager.h"
#include "EditorModeManager.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/DetailLayoutBuilder.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Runtime/Slate/Public/Widgets/Text/STextBlock.h"
#include "Runtime/Launch/Resources/Version.h"
#include "TritonPreprocessorApi.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Materials/Material.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Char.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SButton.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "MeshDescription.h"

#include "RawMesh.h"
#include "LandscapeProxy.h"
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 20
#include "AI/Navigation/RecastNavMesh.h"
#else
#include "Navmesh/RecastNavMesh.h"
#endif
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "Misc/MessageDialog.h"
#include "SourceControlOperations.h"
#include "StaticMeshDescription.h"

// Helper method copied from UE's source in StaticMeshEdit.cpp
// For some reason, linking against UnrealEd isn't finding this function definition
UStaticMesh*
CreateStaticMesh(struct FRawMesh& RawMesh, TArray<FStaticMaterial>& Materials, UObject* InOuter, FName InName)
{
    // Create the UStaticMesh object.
    FStaticMeshComponentRecreateRenderStateContext RecreateRenderStateContext(
        FindObject<UStaticMesh>(InOuter, *InName.ToString()));
    auto StaticMesh = NewObject<UStaticMesh>(InOuter, InName, RF_Public | RF_Standalone);

    // Add one LOD for the base mesh
    FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
    SrcModel.SaveRawMesh(RawMesh);
    StaticMesh->StaticMaterials = Materials;

    int32 NumSections = StaticMesh->StaticMaterials.Num();

    // Set up the SectionInfoMap to enable collision
    for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
    {
        FMeshSectionInfo Info = StaticMesh->GetSectionInfoMap().Get(0, SectionIdx);
        Info.MaterialIndex = SectionIdx;
        Info.bEnableCollision = true;
        StaticMesh->GetSectionInfoMap().Set(0, SectionIdx, Info);
        StaticMesh->GetOriginalSectionInfoMap().Set(0, SectionIdx, Info);
    }

    // Set the Imported version before calling the build
    StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

    StaticMesh->Build();
    StaticMesh->MarkPackageDirty();
    return StaticMesh;
}

// Starting in 4.22, UE changed the first parameter to this function
// We still use the first version when constructing meshes ourself, so leaving both versions in
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 22
UStaticMesh*
CreateStaticMesh(FMeshDescription& RawMesh, TArray<FStaticMaterial>& Materials, UObject* InOuter, FName InName)
{
    // Create the UStaticMesh object.
    FStaticMeshComponentRecreateRenderStateContext RecreateRenderStateContext(
        FindObject<UStaticMesh>(InOuter, *InName.ToString()));
    auto StaticMesh = NewObject<UStaticMesh>(InOuter, InName, RF_Public | RF_Standalone);

    // Add one LOD for the base mesh
    FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
    FMeshDescription* MeshDescription = StaticMesh->CreateMeshDescription(0);
    *MeshDescription = RawMesh;
    StaticMesh->CommitMeshDescription(0);
    StaticMesh->StaticMaterials = Materials;

    int32 NumSections = StaticMesh->StaticMaterials.Num();

    // Set up the SectionInfoMap to enable collision
    for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
    {
        FMeshSectionInfo Info = StaticMesh->GetSectionInfoMap().Get(0, SectionIdx);
        Info.MaterialIndex = SectionIdx;
        Info.bEnableCollision = true;
        StaticMesh->GetSectionInfoMap().Set(0, SectionIdx, Info);
        StaticMesh->GetOriginalSectionInfoMap().Set(0, SectionIdx, Info);
    }

    // Set the Imported version before calling the build
    StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

    StaticMesh->Build();
    StaticMesh->MarkPackageDirty();
    return StaticMesh;
}
#endif

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "SAcousticsProbesTab"

using namespace TritonRuntime;

bool SAcousticsProbesTab::m_CancelRequest = false;
FString SAcousticsProbesTab::m_CurrentStatus = TEXT("");
float SAcousticsProbesTab::m_CurrentProgress = 0.0f;

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAcousticsProbesTab::Construct(const FArguments& InArgs, SAcousticsEdit* ownerEdit)
{
    const FString helpTextTitle = TEXT("Step Three");
    const FString helpText = TEXT(
        "Previewing the probe points helps ensure that probe locations map to the areas in the scene where the user "
        "will travel, as well as evaulating the number of probe points, which affects bake time and cost.\n\nIn "
        "addition, you can preview the voxels to see how portals (doors, windows, etc.) might be affected by the "
        "simulation resolution.The probe points calculated here will be used when you submit your bake.");

    // If python isn't initialized, bail out
    if (!AcousticsSharedState::IsInitialized())
    {
        // clang-format off
        ChildSlot
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Python is required for Project Acoustics baking.\nPlease enable the Python plugin.")))
        ];
        // clang-format on
        return;
    }

    FEditorDelegates::MapChange.AddLambda([](uint32 changeType) {
        if (changeType == MapChangeEventFlags::NewMap)
        {
            m_CurrentStatus = TEXT("");
        }
    });

    m_OwnerEdit = ownerEdit;

#pragma region ProbesTabUI

    // clang-format off
    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SAcousticsEdit::MakeHelpTextWidget(helpTextTitle, helpText)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Simulation Resolution")))
            ]
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            [
                SNew(SComboBox<TSharedPtr<FString>>)
                .IsEnabled(this, &SAcousticsProbesTab::ShouldEnableForProcessing)
                .OptionsSource(&g_ResolutionNames)
                .ToolTipText(FText::FromString(TEXT("Determines the frequency for simulation processing")))
                .OnGenerateWidget(this, &SAcousticsProbesTab::MakeResolutionOptionsWidget)
                .OnSelectionChanged(this, &SAcousticsProbesTab::OnResolutionChanged)
                [
                    SNew(STextBlock)
                    .Text(this, &SAcousticsProbesTab::GetCurrentResolutionLabel)
                    .Font(IDetailLayoutBuilder::GetDetailFont())
                ]
                .InitiallySelectedItem(m_CurrentResolution)
                [
                    SNew(STextBlock)
                    .Font(IDetailLayoutBuilder::GetDetailFont())
                    .Text(this, &SAcousticsProbesTab::GetCurrentResolutionLabel)
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Acoustics Data Folder")))
                .ToolTipText(FText::FromString(TEXT("Path to the acoustics data folder where generated files are stored")))
            ]
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .FillWidth(1)
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            [
                SNew(SEditableTextBox)
                .IsReadOnly(true)
                .Text(this, &SAcousticsProbesTab::GetDataFolderPath)
                .MinDesiredWidth(100.0f)
                .ToolTipText(this, &SAcousticsProbesTab::GetDataFolderPath)
                .AllowContextMenu(true)
            ]
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            [
                SNew(SButton)
                .IsEnabled(this, &SAcousticsProbesTab::ShouldEnableForProcessing)
                .Text(FText::FromString(TEXT("...")))
                .OnClicked(this, &SAcousticsProbesTab::OnAcousticsDataFolderButtonClick)
                .ToolTipText(FText::FromString(TEXT("Select acoustics data folder")))
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Acoustics Files Prefix")))
            ]
            + SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .AutoWidth()
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            [
                SAssignNew(m_PrefixTextBox, SEditableTextBox)
                .IsEnabled(this, &SAcousticsProbesTab::ShouldEnableForProcessing)
                .Text(this, &SAcousticsProbesTab::GetPrefixText)
                .OnTextCommitted(this, &SAcousticsProbesTab::OnPrefixTextChange)
                .MinDesiredWidth(100.0f)
                .ToolTipText(FText::FromString(TEXT("Prefix used when naming generated files")))
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FAcousticsEditSharedProperties::StandardPadding)
        [
            SNew(SSeparator)
            .Orientation(Orient_Horizontal)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            [
                SNew(SButton)
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                .Text(this, &SAcousticsProbesTab::GetCalculateClearText)
                .OnClicked(this, &SAcousticsProbesTab::OnCalculateClearButton)
                .ToolTipText(this, &SAcousticsProbesTab::GetCalculateClearTooltipText)
                .DesiredSizeScale(FVector2D(3.0f, 1.f))
            ]
        ]
		// Added button to the Probes Tab to check out the config and vox files or mark them for add if they aren't already source controlled.
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FAcousticsEditSharedProperties::StandardPadding)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(FText::FromString(TEXT("CheckOut / MarkForAdd Vox and Config File")))
				.ToolTipText(FText::FromString(TEXT("Check out the config and vox files or mark them for add if they aren't already in source control.")))
				.OnClicked(this, &SAcousticsProbesTab::OnCheckOutFilesButton)
				.IsEnabled(this, &SAcousticsProbesTab::CanCheckOutFiles)
			]
		]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FAcousticsEditSharedProperties::StandardPadding)
        [
            SNew(SProgressBar)
            .Percent(this, &SAcousticsProbesTab::GetProgressBarPercent)
            .Visibility(this, &SAcousticsProbesTab::GetProgressBarVisibility)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FAcousticsEditSharedProperties::StandardPadding)
        [
            SNew(STextBlock)
            .AutoWrapText(true)
            .Text_Lambda([this]() { return FText::FromString(m_CurrentStatus); })
        ]
    ];
    // clang-format on
#pragma endregion
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> SAcousticsProbesTab::MakeResolutionOptionsWidget(TSharedPtr<FString> inString)
{
    return SNew(STextBlock)
        .Font(IDetailLayoutBuilder::GetDetailFont())
        .Text(FText::FromString(*inString))
        .IsEnabled(true);
}

void SAcousticsProbesTab::OnResolutionChanged(TSharedPtr<FString> selection, ESelectInfo::Type info)
{
    auto params = AcousticsSharedState::GetSimulationParameters();
    params.frequency = g_ResolutionFrequencies[static_cast<uint8>(LabelToResolution(selection))];
    AcousticsSharedState::SetSimulationParameters(params);
}

FText SAcousticsProbesTab::GetCurrentResolutionLabel() const
{
    auto params = AcousticsSharedState::GetSimulationParameters();
    auto resolution = FrequencyToResolution(params.frequency);
    return FText::FromString(*g_ResolutionNames[static_cast<uint8>(resolution)]);
}

FText SAcousticsProbesTab::GetCalculateClearText() const
{
    auto text = TEXT("Clear");
    auto simConfig = AcousticsSharedState::GetSimulationConfiguration();
    if (simConfig)
    {
        if (simConfig->GetState() == SimulationConfigurationState::Failed)
        {
            AcousticsSharedState::SetSimulationConfiguration(nullptr);
            ResetPrebakeCalculationState();
            UE_LOG(LogAcoustics, Error, TEXT("Failed to place simulation probes, please check your settings."));
            m_OwnerEdit->SetError(TEXT("Failed to place simulation probes, please check your settings."));
        }
        else
        {
            text = simConfig->IsReady() ? TEXT("Clear") : TEXT("Cancel");
        }
    }
    else
    {
        text = TEXT("Calculate");
    }
    return FText::FromString(text);
}

FText SAcousticsProbesTab::GetCalculateClearTooltipText() const
{
    auto text = TEXT("");
    auto simConfig = AcousticsSharedState::GetSimulationConfiguration();
    if (simConfig)
    {
        text = simConfig->IsReady() ? TEXT("Delete previously processed configuration")
                                    : TEXT("Cancel configuration processing");
    }
    else
    {
        text = TEXT("Generate simulation configuration");
    }
    return FText::FromString(text);
}

FReply SAcousticsProbesTab::OnCalculateClearButton()
{
    // No configuration, we need to run pre-bake
    if (AcousticsSharedState::GetSimulationConfiguration() == nullptr)
    {
        auto config = AcousticsSharedState::GetProjectConfiguration();
        if (!FPaths::DirectoryExists(config.content_dir))
        {
            auto created = FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*config.content_dir);
            if (!created)
            {
                TCHAR error[] = TEXT("Could not create acoustics data folder. Please choose a new location");
                UE_LOG(LogAcoustics, Error, error);
                m_OwnerEdit->SetError(error);
                return FReply::Handled();
            }
        }
        if (config.content_dir.IsEmpty())
        {
            TCHAR error[] = TEXT("Please specify an acoustics data folder");
            UE_LOG(LogAcoustics, Error, error);
            m_OwnerEdit->SetError(error);
        }
        else
        {
            // Clear the error text (if set) before starting pre-bake calculations
            m_OwnerEdit->SetError(TEXT(""));
            m_CancelRequest = false;
            ComputePrebake();
        }
    }
    // Have existing pre-bake data, need to clear it
    else
    {
        // Set the read-only flag for the config and vox files to false so that they can be deleted.
        FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*AcousticsSharedState::GetVoxFilepath(), false);
        FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*AcousticsSharedState::GetConfigFilepath(), false);
        // Delete vox and config files
        FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*AcousticsSharedState::GetVoxFilepath());
        FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*AcousticsSharedState::GetConfigFilepath());
        // Check if files were successfully deleted.
        if (FPaths::FileExists(AcousticsSharedState::GetVoxFilepath()) ||
            FPaths::FileExists(AcousticsSharedState::GetConfigFilepath()))
        {
            FMessageDialog::Open(
                EAppMsgType::Ok,
                FText::FromString(
                    "Unable to delete files " + AcousticsSharedState::GetVoxFilepath() + " and " +
                    AcousticsSharedState::GetConfigFilepath() +
                    ". Make sure the files are not open in another application and are allowed to be deleted and try "
                    "again."));
            return FReply::Handled();
        }
        // Set the cancel request and wait for it to take effect
        m_CancelRequest = true;
        AcousticsSharedState::SetSimulationConfiguration(nullptr);
        // Now reset everything else
        ResetPrebakeCalculationState();
    }

    return FReply::Handled();
}

// Definitions for the functions associated with the check out button.
FReply SAcousticsProbesTab::OnCheckOutFilesButton()
{
    CheckOutVoxAndConfigFile();
    return FReply::Handled();
}

void SAcousticsProbesTab::CheckOutVoxAndConfigFile()
{
    ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
    if (ISourceControlModule::Get().IsEnabled() && SourceControlProvider.IsAvailable())
    {
        TArray<FString> FilesToCheckOut;
        FilesToCheckOut.Add(AcousticsSharedState::GetVoxFilepath());
        FilesToCheckOut.Add(AcousticsSharedState::GetConfigFilepath());
        USourceControlHelpers::CheckOutFiles(FilesToCheckOut);
    }
}

bool SAcousticsProbesTab::CanCheckOutFiles() const
{
    ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
    // source control not available.
    if (!ISourceControlModule::Get().IsEnabled() || !SourceControlProvider.IsAvailable())
    {
        return false;
    }

    // if button text says 'clear', that means files have been generated, and thus, can be checked out.
    if (GetCalculateClearText().EqualTo(FText::FromString(TEXT("Clear"))))
    {
        return true;
    }

    return false;
}

FText SAcousticsProbesTab::GetPrefixText() const
{
    return FText::FromString(AcousticsSharedState::GetConfigurationPrefixForLevel());
}

void SAcousticsProbesTab::OnPrefixTextChange(const FText& newText, ETextCommit::Type commitInfo)
{
    // Do nothing if we aborted
    if (commitInfo == ETextCommit::OnEnter || commitInfo == ETextCommit::OnUserMovedFocus)
    {
        auto newString = newText.ToString();
        // Check if the string contains unsupported characters
        for (auto i = 0; i < newString.Len(); ++i)
        {
            if (!FChar::IsAlpha(newString[i]) && !FChar::IsDigit(newString[i]) && newString[i] != TEXT('_'))
            {
                m_PrefixTextBox->SetText(newText);
                FSlateApplication::Get().SetKeyboardFocus(m_PrefixTextBox);
                m_OwnerEdit->SetError(TEXT("Prefix can only contain letters, numbers and underscores"));
                return;
            }
        }
        AcousticsSharedState::SetConfigurationPrefixForLevel(newString);
        m_OwnerEdit->SetError(TEXT(""));
    }
}

FText SAcousticsProbesTab::GetDataFolderPath() const
{
    return FText::FromString(AcousticsSharedState::GetProjectConfiguration().content_dir);
}

FReply SAcousticsProbesTab::OnAcousticsDataFolderButtonClick()
{
    auto desktopPlatform = FDesktopPlatformModule::Get();
    if (desktopPlatform)
    {
        FString folderName;
        desktopPlatform->OpenDirectoryDialog(
            FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
            TEXT("Acoustics Data Folder"),
            AcousticsSharedState::GetProjectConfiguration().content_dir,
            folderName);

        if (!folderName.IsEmpty())
        {
            auto config = AcousticsSharedState::GetProjectConfiguration();
            config.content_dir = folderName;
            AcousticsSharedState::SetProjectConfiguration(config);
            m_OwnerEdit->SetError(TEXT(""));
        }
    }
    return FReply::Handled();
}

// Closely based on: UnFbx::FFbxImporter::BuildStaticMeshFromGeometry()
UStaticMesh* ConstructStaticMeshGeo(const TArray<FVector>& verts, const TArray<int32>& indices, FName meshName)
{
    int32 triangleCount = indices.Num() / 3;
    int32 wedgeCount = triangleCount * 3;

    FRawMesh rawMesh;
    rawMesh.FaceMaterialIndices.AddZeroed(triangleCount);
    rawMesh.FaceSmoothingMasks.AddZeroed(triangleCount);
    rawMesh.WedgeIndices.AddZeroed(wedgeCount);
    rawMesh.WedgeTexCoords[0].AddZeroed(wedgeCount);

    TMap<int32, int32> indexMap;
    for (int32 triangleIndex = 0; triangleIndex < triangleCount; triangleIndex++)
    {
        for (int32 cornerIndex = 0; cornerIndex < 3; cornerIndex++)
        {
            int32 wedgeIndex = triangleIndex * 3 + cornerIndex;

            // Store vertex index and position.
            int32 controlPointIndex = indices[wedgeIndex]; // Mesh->GetPolygonVertex(TriangleIndex, CornerIndex);
            int32* existingIndex = indexMap.Find(controlPointIndex);
            if (existingIndex)
            {
                rawMesh.WedgeIndices[wedgeIndex] = *existingIndex;
            }
            else
            {
                int32 vertexIndex = rawMesh.VertexPositions.Add(verts[controlPointIndex]);
                rawMesh.WedgeIndices[wedgeIndex] = vertexIndex;
                indexMap.Add(controlPointIndex, vertexIndex);
            }

            // normals, tangents and binormals : SKIP
            // vertex colors : SKIP

            // uvs: we don't care about these, but these are required for a legal mesh
            rawMesh.WedgeTexCoords[0][wedgeIndex].X = 0.0f;
            rawMesh.WedgeTexCoords[0][wedgeIndex].Y = 0.0f;
        }
        // smoothing mask : SKIP
        // uvs: taken care of above.

        // material index
        rawMesh.FaceMaterialIndices[triangleIndex] = 0;
    }

    TArray<FStaticMaterial> mats;
    mats.Add(FStaticMaterial(CastChecked<UMaterialInterface>(UMaterial::GetDefaultMaterial(MD_Surface))));

    return CreateStaticMesh(rawMesh, mats, GetTransientPackage(), meshName);
}

static const FName c_NavMeshName(TEXT("TritonNavigableArea"));

UStaticMesh* ExtractStaticMeshFromNavigationMesh(const ARecastNavMesh* navMeshActor, UWorld* world)
{
    check(navMeshActor != nullptr);

    // Extract out navmesh triangulated geo
    // Code motivated from UNavMeshRenderingComponent::GatherData() >>> if (NavMesh->bDrawTriangleEdges)...
    TArray<FVector> navVerts;
    TArray<int32> navIndices;

    FRecastDebugGeometry geom;
    navMeshActor->GetDebugGeometry(geom);

    // Collect all the vertices
    for (auto& vert : geom.MeshVerts)
    {
        navVerts.Add(vert);
    }

    // Collect all the indices
    for (int32 areaIdx = 0; areaIdx < RECAST_MAX_AREAS; ++areaIdx)
    {
        for (int32 idx : geom.AreaIndices[areaIdx])
        {
            navIndices.Add(idx);
        }
    }

    // Create static mesh from nav mesh data
    UStaticMesh* staticMesh = ConstructStaticMeshGeo(navVerts, navIndices, c_NavMeshName);

    if (!staticMesh)
    {
        UE_LOG(LogAcoustics, Error, TEXT("Failed while creating static mesh from nav mesh data"));
        return nullptr;
    }

    return staticMesh;
}

TritonMaterialCode GetMaterialCodeForFace(
    const UStaticMesh* mesh, const TArray<UMaterialInterface*>& materials, uint32 face,
    TArray<uint32>& materialIDsNotFound)
{
    UMaterialInterface* material = nullptr;
    const auto& renderData = mesh->GetLODForExport(0);
    auto sectionCount = renderData.Sections.Num();

    auto totalTriangles = 0u;
    for (const auto& section : renderData.Sections)
    {
        if (face >= totalTriangles && face < (totalTriangles + section.NumTriangles) &&
            section.MaterialIndex < materials.Num())
        {
            // We've found the material for this face, so save it and move on.
            material = materials[section.MaterialIndex];
            break;
        }
        totalTriangles += section.NumTriangles;
    }

    TritonMaterialCode code = TRITON_DEFAULT_WALL_CODE;
    if (material && AcousticsSharedState::GetMaterialsLibrary())
    {
        if (!AcousticsSharedState::GetMaterialsLibrary()->FindMaterialCode(material->GetName(), &code) &&
            !materialIDsNotFound.Contains(material->GetUniqueID()))
        {
            UE_LOG(
                LogAcoustics,
                Error,
                TEXT("The material %s has no acoustic material mapping (it did not show up in the materials "
                     "mapping tab), but is used by a mesh. Using the default code."),
                *(material->GetName()));

            materialIDsNotFound.Add(material->GetUniqueID());
        }
    }

    return code;
}

void SAcousticsProbesTab::AddStaticMeshToAcousticMesh(
    AcousticMesh* acousticMesh, AActor* actor, const UStaticMesh* mesh, const TArray<UMaterialInterface*>& materials,
    MeshType type, TArray<uint32>& materialIDsNotFound)
{
    TArray<ATKVectorF> vertices;
    TArray<TritonAcousticMeshTriangleInformation> triangleInfos;

    if (mesh == nullptr)
    {
        return;
    }
    // From the acoustics ed mode, get the pointer to the materials tab and get a read-only reference to the material
    // items list.
    FAcousticsEdMode* AcousticsEdMode =
        static_cast<FAcousticsEdMode*>(GLevelEditorModeTools().GetActiveMode(FAcousticsEdMode::EM_AcousticsEdModeId));
    const TArray<TSharedPtr<MaterialItem>>& MaterialItemsList =
        AcousticsEdMode->GetMaterialsTab()->GetMaterialItemsList();

    const auto checkHasVerts = true;
    const auto LOD = 0;
    if (!mesh->HasValidRenderData(checkHasVerts, LOD))
    {
        UE_LOG(
            LogAcoustics,
            Error,
            TEXT("Error while adding static mesh [%s], there is no valid render data for LOD %d. Ignoring."),
            *mesh->GetName(),
            LOD);
    }

    const auto& renderData = mesh->GetLODForExport(LOD);
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 20
    const auto& vertexBuffer = renderData.PositionVertexBuffer;
#else
    const auto& vertexBuffer = renderData.VertexBuffers.PositionVertexBuffer;
#endif
    auto indexBuffer = renderData.IndexBuffer.GetArrayView();
    auto triangleCount = renderData.GetNumTriangles();
    auto vertexCount = vertexBuffer.GetNumVertices();
    for (auto i = 0u; i < vertexCount; ++i)
    {
        // If actor is provided, its actor-to-world transform is used,
        // otherwise vertices are interpreted to be directly in world coordinates
        const auto& vertexPos = vertexBuffer.VertexPosition(i);
        const auto& vertexWorld = actor == nullptr ? vertexPos : actor->GetTransform().TransformPosition(vertexPos);
        auto vertex = UnrealPositionToTriton(vertexWorld);
        vertices.Add(ATKVectorF{vertex.X, vertex.Y, vertex.Z});
    }

    for (auto triangle = 0; triangle < triangleCount; ++triangle)
    {
        auto index1 = indexBuffer[(triangle * 3) + 0];
        auto index2 = indexBuffer[(triangle * 3) + 1];
        auto index3 = indexBuffer[(triangle * 3) + 2];

        TritonAcousticMeshTriangleInformation triangleInfo;
        triangleInfo.Indices = ATKVectorI{static_cast<int>(index1), static_cast<int>(index2), static_cast<int>(index3)};
        // Only lookup material codes for geometry meshes.
        if (type == MeshTypeGeometry)
        {
            // Cache off the material code for this triangle.
            TritonMaterialCode MaterialCode = GetMaterialCodeForFace(mesh, materials, triangle, materialIDsNotFound);

            // If there are any material override volumes, check those first
            bool useOverride = false;
            for (auto i = 0; i < m_MaterialOverrideVolumes.Num(); i++)
            {
                // See if any of the triangle vertices is inside or on the override volume
                // if any of them are, use this override value
                AAcousticsProbeVolume* overrideVolume = m_MaterialOverrideVolumes[i];
                if (IsOverlapped(overrideVolume, vertices[index1], vertices[index2], vertices[index3]))
                {
                    // Using the override material name prefix.
                    if (!AcousticsSharedState::GetMaterialsLibrary()->FindMaterialCode(
                            (AAcousticsProbeVolume::OverrideMaterialNamePrefix + overrideVolume->MaterialName),
                            &triangleInfo.MaterialCode))
                    {
                        UE_LOG(
                            LogAcoustics,
                            Error,
                            TEXT("The material %s has no acoustic material mapping (it did not show up in the "
                                 "materials mapping tab), but is used by a mesh. Using the default code."),
                            // Using the override material name prefix.
                            *(AAcousticsProbeVolume::OverrideMaterialNamePrefix + overrideVolume->MaterialName));
                    }

                    useOverride = true;
                    break;
                }
            }
            // Implemented calculations for remap volumes.
            bool useRemap = false;
            // remap volumes calculations
            for (int32 i = 0; i < m_MaterialRemapVolumes.Num(); ++i)
            {
                // See if any of the triangle vertices is inside or on the remap volume
                // if any of them are, and the material is supposed to be remapped, do it
                AAcousticsProbeVolume* remapVolume = m_MaterialRemapVolumes[i];
                if (IsOverlapped(remapVolume, vertices[index1], vertices[index2], vertices[index3]))
                {
                    useRemap = true;

                    TritonAcousticMaterial AcousticMaterial;
                    if (!TritonPreprocessor_MaterialLibrary_GetMaterialInfo(
                            AcousticsSharedState::GetMaterialsLibrary()->GetHandle(), MaterialCode, &AcousticMaterial))
                    {
                        useRemap = false;
                        break;
                    }

                    FString acousticMaterialToRemap;
                    for (const TSharedPtr<MaterialItem>& Item : MaterialItemsList)
                    {
                        if (Item->UEMaterialName == AcousticMaterial.Name)
                        {
                            acousticMaterialToRemap = Item->AcousticMaterialName;
                            break;
                        }
                    }

                    FString* RemappedMaterialName = remapVolume->MaterialRemapping.Find(acousticMaterialToRemap);
                    if (RemappedMaterialName == nullptr)
                    {
                        useRemap = false;
                        break;
                    }

                    FString RemappedAcousticMaterialName =
                        AAcousticsProbeVolume::RemapMaterialNamePrefix + *RemappedMaterialName;

                    if (!AcousticsSharedState::GetMaterialsLibrary()->FindMaterialCode(
                            RemappedAcousticMaterialName, &triangleInfo.MaterialCode))
                    {
                        useRemap = false;
                        UE_LOG(
                            LogAcoustics,
                            Error,
                            TEXT("Invalid acoustic material %s found in the AcousticMaterialRemapping volume %s."),
                            *RemappedAcousticMaterialName,
                            *(remapVolume->GetName()));
                    }

                    break;
                }
            }
            if (!useOverride && !useRemap)
            {
                triangleInfo.MaterialCode = MaterialCode;
            }
        }
        else
        {
            // Metadata meshes like nav meshes will ignore material, provide default.
            triangleInfo.MaterialCode = TRITON_DEFAULT_WALL_CODE;
        }
        triangleInfos.Add(triangleInfo);
    }

    if (type == MeshTypeProbeSpacingVolume)
    {
        auto probeVol = dynamic_cast<AAcousticsProbeVolume*>(actor);
        acousticMesh->AddProbeSpacingVolume(
            vertices.GetData(),
            vertices.Num(),
            triangleInfos.GetData(),
            triangleInfos.Num(),
            probeVol->MaxProbeSpacing);
    }
    else
    {
        acousticMesh->Add(vertices.GetData(), vertices.Num(), triangleInfos.GetData(), triangleInfos.Num(), type);
    }
}

void SAcousticsProbesTab::AddLandscapeToAcousticMesh(
    AcousticMesh* acousticMesh, ALandscapeProxy* actor, MeshType type, TArray<uint32>& materialIDsNotFound)
{
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 22
    FRawMesh rawMesh;
#else
    FMeshDescription rawMesh;
    FStaticMeshAttributes(rawMesh).Register();
#endif
    if (!actor->ExportToRawMesh(actor->ExportLOD, rawMesh))
    {
        UE_LOG(
            LogAcoustics,
            Error,
            TEXT("Failed to export raw mesh for landscape actor: [%s]. Ignoring."),
            *actor->GetName());
        return;
    }

    TArray<FStaticMaterial> mats;
    UMaterialInterface* landscapeMat = actor->GetLandscapeMaterial();
    if (landscapeMat != nullptr)
    {
        mats.Add(FStaticMaterial(landscapeMat));
    }
    else
    {
        mats.Add(FStaticMaterial(CastChecked<UMaterialInterface>(UMaterial::GetDefaultMaterial(MD_Surface))));
    }

    auto* staticMesh = CreateStaticMesh(rawMesh, mats, GetTransientPackage(), *actor->GetName());

    TArray<UMaterialInterface*> finalMats;
    finalMats.Add(staticMesh->GetMaterial(0));

    AddStaticMeshToAcousticMesh(acousticMesh, nullptr, staticMesh, finalMats, type, materialIDsNotFound);
}

void SAcousticsProbesTab::AddVolumeToAcousticMesh(
    AcousticMesh* acousticMesh, AAcousticsProbeVolume* actor, TArray<uint32>& materialIDsNotFound)
{
    TArray<UMaterialInterface*> emptyMaterials;

    MeshType type = MeshTypeInvalid;
    if (actor->VolumeType == AcousticsVolumeType::Include)
    {
        type = MeshTypeIncludeVolume;
    }
    else if (actor->VolumeType == AcousticsVolumeType::Exclude)
    {
        type = MeshTypeExcludeVolume;
    }
    else if (
        actor->VolumeType == AcousticsVolumeType::MaterialOverride ||
        actor->VolumeType == AcousticsVolumeType::MaterialRemap)
    {
        // Do not pass these volumes into Triton. We instead use them to set material properties on static meshes
        return;
    }
    else if (actor->VolumeType == AcousticsVolumeType::ProbeSpacing)
    {
        type = MeshTypeProbeSpacingVolume;
    }
    else
    {
        UE_LOG(LogAcoustics, Error, TEXT("[Volume: %s] Unknown mesh type for volume. Ignoring."), *actor->GetName());
        return;
    }

    // Create static mesh from brush
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 22
    FRawMesh Mesh;
#else
    FMeshDescription Mesh;
    FStaticMeshAttributes MeshAttributes(Mesh);
    MeshAttributes.Register();
#endif
    TArray<FStaticMaterial> Materials;
    // Pass a null actor pointer, so brush geo doesn't bake-in actor transforms, we take care
    // of that below when its static mesh is exported as part of the actor. Passing in the actor
    // here would apply the actor transform twice.
    GetBrushMesh(nullptr, actor->Brush, Mesh, Materials);

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 22
    if (Mesh.VertexPositions.Num() == 0)
#else
    if (Mesh.Vertices().Num() == 0)
#endif
    {
        UE_LOG(
            LogAcoustics,
            Error,
            TEXT("[Volume: %s] Mesh created from volume's brush has zero vertex count. Ignoring."),
            *actor->GetName());
        return;
    }

    UStaticMesh* StaticMesh = CreateStaticMesh(Mesh, Materials, GetTransientPackage(), actor->GetFName());

    if (!StaticMesh)
    {
        UE_LOG(
            LogAcoustics,
            Error,
            TEXT("[Volume: %s] Failed to create static mesh from volume's raw mesh. Ignoring."),
            *actor->GetName());
        return;
    }

    // This exports the static mesh using the volume actor's transforms
    AddStaticMeshToAcousticMesh(acousticMesh, actor, StaticMesh, emptyMaterials, type, materialIDsNotFound);
}

void SAcousticsProbesTab::AddPinnedProbeToAcousticMesh(AcousticMesh* acousticMesh, const FVector& probeLocation)
{
    acousticMesh->AddPinnedProbe(ATKVectorF{probeLocation.X, probeLocation.Y, probeLocation.Z});
}

void SAcousticsProbesTab::AddNavmeshToAcousticMesh(
    AcousticMesh* acousticMesh, ARecastNavMesh* navActor, TArray<UMaterialInterface*> materials,
    TArray<uint32>& materialIDsNotFound)
{
    auto staticMesh = ExtractStaticMeshFromNavigationMesh(navActor, GEditor->GetWorld());
    if (!staticMesh)
    {
        return;
    }

    const auto checkHasVerts = true;
    const auto LOD = 0;
    if (staticMesh->HasValidRenderData(checkHasVerts, LOD))
    {
        AddStaticMeshToAcousticMesh(
            acousticMesh, navActor, staticMesh, materials, MeshTypeNavigation, materialIDsNotFound);
        return;
    }

    UE_LOG(
        LogAcoustics,
        Warning,
        TEXT("Nav mesh [%s] has no valid render data for LOD %d. Triggering navigation build..."),
        *navActor->GetName(),
        LOD);

    // trigger navigation rebuild and block on it so we can export it
    navActor->RebuildAll();
    navActor->EnsureBuildCompletion();

    auto staticMeshRebuilt = ExtractStaticMeshFromNavigationMesh(navActor, GEditor->GetWorld());
    if (staticMeshRebuilt != nullptr && staticMeshRebuilt->HasValidRenderData(checkHasVerts, LOD))
    {
        UE_LOG(LogAcoustics, Log, TEXT("Nav mesh [%s] successfully rebuilt."), *navActor->GetName());
        AddStaticMeshToAcousticMesh(
            acousticMesh, navActor, staticMeshRebuilt, materials, MeshTypeNavigation, materialIDsNotFound);
    }
    else
    {
        UE_LOG(
            LogAcoustics,
            Error,
            TEXT("Automatic rebuild of nav mesh [%s] failed, investigate in editor. Ignoring and "
                 "continuing."),
            *navActor->GetName());
    }
}

void SAcousticsProbesTab::ComputePrebake()
{
    // First, collect all the Acoustic Material Override volumes
    // We use these later to help figure out what material to assign to a mesh
    m_MaterialOverrideVolumes.Empty();
    // Also collect the Acoustic Material Remap volumes.
    m_MaterialRemapVolumes.Empty();
    for (TActorIterator<AActor> itr(GEditor->GetEditorWorldContext().World()); itr; ++itr)
    {
        auto actor = *itr;
        if (actor->IsA<AAcousticsProbeVolume>())
        {
            AAcousticsProbeVolume* volume = Cast<AAcousticsProbeVolume>(actor);
            if (volume->VolumeType == AcousticsVolumeType::MaterialOverride)
            {
                m_MaterialOverrideVolumes.Add(volume);
            }
            // Check material remap volumes as well.
            else if (volume->VolumeType == AcousticsVolumeType::MaterialRemap)
            {
                m_MaterialRemapVolumes.Add(volume);
            }
        }
    }

    // Used to track any materials that aren't properly mapped
    // Will display error text to help with debugging
    TArray<uint32> materialIDsNotFound;
    TArray<UMaterialInterface*> emptyMaterials;

    // Create the acoustic mesh
    TSharedPtr<AcousticMesh> acousticMesh = MakeShareable<AcousticMesh>(AcousticMesh::Create().Release());

    for (TActorIterator<AActor> itr(GEditor->GetEditorWorldContext().World()); itr; ++itr)
    {
        auto actor = *itr;
        const auto acousticGeometryTag = actor->ActorHasTag(c_AcousticsGeometryTag);
        const auto acousticNavigationTag = actor->ActorHasTag(c_AcousticsNavigationTag);

        if (acousticNavigationTag)
        {
            // Nav Meshes
            if (actor->IsA<ARecastNavMesh>())
            {
                AddNavmeshToAcousticMesh(
                    acousticMesh.Get(), Cast<ARecastNavMesh>(actor), emptyMaterials, materialIDsNotFound);
            }
            // Volumes
            else if (actor->IsA<AAcousticsProbeVolume>())
            {
                AddVolumeToAcousticMesh(acousticMesh.Get(), Cast<AAcousticsProbeVolume>(actor), materialIDsNotFound);
            }
            // Pinned probes
            else if (actor->IsA<AAcousticsPinnedProbe>())
            {
                auto probeLoc = TritonRuntime::UnrealPositionToTriton(actor->GetActorLocation());
                AddPinnedProbeToAcousticMesh(acousticMesh.Get(), probeLoc);
            }
            else if (actor->IsA<AStaticMeshActor>())
            {
                UStaticMeshComponent* meshComponent;
                meshComponent = Cast<AStaticMeshActor>(actor)->GetStaticMeshComponent();

                if (meshComponent != nullptr)
                {
                    // This actor may override materials on the associated static mesh, so make sure we use the correct
                    // set.
                    TArray<UMaterialInterface*> materials = meshComponent->GetMaterials();

                    // Static meshes can be tagged for both AcousticsGeometry and AcousticsNavigation
                    // If that's the case, we need to make a copy of their geometry before adding it to the AcousticMesh
                    // It's not supported to have the same geometry contain both tags internally
                    AddStaticMeshToAcousticMesh(
                        acousticMesh.Get(),
                        actor,
                        meshComponent->GetStaticMesh(),
                        materials,
                        MeshTypeNavigation,
                        materialIDsNotFound);
                }
            }
            // Search components
            else
            {
                // dynamic openings
                auto* openingComponent = actor->FindComponentByClass<UAcousticsDynamicOpening>();
                if (openingComponent)
                {
                    FVector ProbeLoc;
                    if (openingComponent->ComputeCenter(ProbeLoc))
                    {
                        AddPinnedProbeToAcousticMesh(acousticMesh.Get(), ProbeLoc);
                    }
                    else
                    {
                        UE_LOG(
                            LogAcoustics,
                            Error,
                            TEXT("Failed to add probe for dynamic opening in actor: [%s]. Dynamic opening will "
                                 "probably mal-function "
                                 "during "
                                 "gameplay."),
                            *actor->GetName());
                    }
                }
            }
        }

        // Static Meshes
        if (acousticGeometryTag)
        {
            if (actor->IsA<AStaticMeshActor>())
            {
                UStaticMeshComponent* meshComponent;
                meshComponent = Cast<AStaticMeshActor>(actor)->GetStaticMeshComponent();

                if (meshComponent != nullptr)
                {
                    // This actor may override materials on the associated static mesh, so make sure we use the correct
                    // set.
                    TArray<UMaterialInterface*> materials = meshComponent->GetMaterials();

                    AddStaticMeshToAcousticMesh(
                        acousticMesh.Get(),
                        actor,
                        meshComponent->GetStaticMesh(),
                        materials,
                        MeshTypeGeometry,
                        materialIDsNotFound);
                }
            }
            // Landscapes
            else if (actor->IsA<ALandscapeProxy>())
            {
                if (acousticNavigationTag)
                {
                    AddLandscapeToAcousticMesh(
                        acousticMesh.Get(), Cast<ALandscapeProxy>(actor), MeshTypeNavigation, materialIDsNotFound);
                }
                if (acousticGeometryTag)
                {
                    AddLandscapeToAcousticMesh(
                        acousticMesh.Get(), Cast<ALandscapeProxy>(actor), MeshTypeGeometry, materialIDsNotFound);
                }
            }
            else
            {
                UE_LOG(LogAcoustics, Error, TEXT("Unsupported Actor tagged for Acoustics: %s"), *actor->GetName());
            }
        }
    }

    // Empty the override volumes list once it's done being used, so
    // that we don't have to assume and depend on the mode deactivation code to clear it.
    m_MaterialOverrideVolumes.Empty();
    // Also empty the material remap volumes.
    m_MaterialRemapVolumes.Empty();

    if (!acousticMesh->HasNavigationMesh())
    {
        UE_LOG(LogAcoustics, Error, TEXT("Need at least one object tagged for Navigation."));
        m_OwnerEdit->SetError(TEXT("Need at least one object tagged for Navigation to represent ground."));
        return;
    }

#ifdef ENABLE_COLLISION_SUPPORT
    // Add collision geometry from selected actors to acoustic mesh as acoustic geometry
    if (!CollisionGeometryToAcousticMeshConverter::AddCollisionGeometryToAcousticMesh(acousticMesh.Get()))
    {
        UE_LOG(LogAcoustics, Error, TEXT("Failed to add collision meshes to the acoustic mesh."));
        m_OwnerEdit->SetError(TEXT("Failed to add collision meshes to the acoustic mesh."));
        return;
    }
#endif // ENABLE_COLLISION_SUPPORT
    // Publish the material library after adding the remap and override materials
    FAcousticsEdMode* AcousticsEdMode =
        static_cast<FAcousticsEdMode*>(GLevelEditorModeTools().GetActiveMode(FAcousticsEdMode::EM_AcousticsEdModeId));
    AcousticsEdMode->GetMaterialsTab()->PublishMaterialLibrary();

    auto config = AcousticsSimulationConfiguration::Create(
        acousticMesh,
        AcousticsSharedState::GetTritonSimulationParameters(),
        AcousticsSharedState::GetTritonOperationalParameters(),
        AcousticsSharedState::GetMaterialsLibrary(),
        true,
        &SAcousticsProbesTab::ComputePrebakeCallback);
    if (config)
    {
        AcousticsSharedState::SetSimulationConfiguration(MoveTemp(config));
        if (materialIDsNotFound.Num() > 0)
        {
            m_OwnerEdit->SetError(TEXT("Unmapped materials exist! See Output Log."));
        }
        else
        {
            m_OwnerEdit->SetError(TEXT(""));
        }
    }
    else
    {
        UE_LOG(LogAcoustics, Error, TEXT("Failed to create simulation config"));
        m_OwnerEdit->SetError(TEXT("Failed to create simulation config"));
    }
}

bool SAcousticsProbesTab::ShouldEnableForProcessing() const
{
    return (AcousticsSharedState::GetSimulationConfiguration() == nullptr);
}

bool SAcousticsProbesTab::ComputePrebakeCallback(char* message, int progress)
{
    FString uMessage(ANSI_TO_TCHAR(message));
    UE_LOG(LogAcoustics, Display, TEXT("%s"), *uMessage);
    m_CurrentStatus = uMessage;
    m_CurrentProgress = (progress / 100.0f);
    return m_CancelRequest;
}

TOptional<float> SAcousticsProbesTab::GetProgressBarPercent() const
{
    return m_CurrentProgress;
}

EVisibility SAcousticsProbesTab::GetProgressBarVisibility() const
{
    return (m_CurrentProgress > 0 && m_CurrentProgress < 1) ? EVisibility::Visible : EVisibility::Collapsed;
}

void SAcousticsProbesTab::ResetPrebakeCalculationState()
{
    m_CancelRequest = false;
    m_CurrentStatus = TEXT("");
    m_CurrentProgress = 0;
}

bool SAcousticsProbesTab::IsOverlapped(
    const AAcousticsProbeVolume* ProbeVolume, const ATKVectorF& Vertex1, const ATKVectorF& Vertex2,
    const ATKVectorF& Vertex3)
{
    auto bounds = ProbeVolume->GetBounds();
    auto boundsBox = bounds.GetBox();
    return boundsBox.IsInsideOrOn(TritonPositionToUnreal(FVector{Vertex1.x, Vertex1.y, Vertex1.z})) ||
           boundsBox.IsInsideOrOn(TritonPositionToUnreal(FVector{Vertex2.x, Vertex2.y, Vertex2.z})) ||
           boundsBox.IsInsideOrOn(TritonPositionToUnreal(FVector{Vertex3.x, Vertex3.y, Vertex3.z}));
}
#undef LOCTEXT_NAMESPACE