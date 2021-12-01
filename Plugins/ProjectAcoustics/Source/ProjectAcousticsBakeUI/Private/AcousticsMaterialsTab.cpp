// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "AcousticsMaterialsTab.h"
#include "AcousticsSharedState.h"
#include "SAcousticsEdit.h"
#include "AcousticsEdMode.h"
#include "AcousticsProbeVolume.h"
#include "AcousticsMaterialUserData.h"
#include "FMaterialRow.h"
#include "Fonts/SlateFontInfo.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "Widgets/Text/STextBlock.h"
#include "Materials/Material.h"
#include "SlateOptMacros.h" // BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
#include "EngineUtils.h"    // FActorIterator
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "Misc/ConfigCacheIni.h"
#include "EditorModeManager.h"
#include "SourceControlHelpers.h"

#define LOCTEXT_NAMESPACE "SAcousticsBakeTab"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

FName SAcousticsMaterialsTab::ColumnNameMaterial(TEXT("Material"));
FName SAcousticsMaterialsTab::ColumnNameAcoustics(TEXT("Acoustics"));
FName SAcousticsMaterialsTab::ColumnNameAbsorption(TEXT("Absorption"));

void SAcousticsMaterialsTab::Construct(const FArguments& InArgs)
{
    FSlateFontInfo StandardFont = FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont"));
    m_AcousticsEditMode =
        static_cast<FAcousticsEdMode*>(GLevelEditorModeTools().GetActiveMode(FAcousticsEdMode::EM_AcousticsEdModeId));

    const FText column1HeaderText = LOCTEXT("MaterialColumnHeader", "Material");
    const FText column2HeaderText = LOCTEXT("AcousticsColumnHeader", "Acoustics");
    const FText column3HeaderText = LOCTEXT("AbsorptionColumnHeader", "Absorption");

    InitKnownMaterialsList();
    UpdateUEMaterials();
    PublishMaterialLibrary();

    auto helpTextTitle = TEXT("Step Two");
    auto helpText = TEXT("Assign acoustic properties to each scene material using the dropdown."
                         "Different materials can have a dramatic effect on the results of the bake. "
                         "Choose \"Custom\" to set the absorption coefficient directly.");

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
        .FillHeight(1)
        [
            SNew(SBox)
            .VAlign(VAlign_Fill)
            [
                SAssignNew(m_ListView, SListView< TSharedPtr<MaterialItem> >)
                .ItemHeight(24)
                .ListItemsSource(&m_Items)
                .OnGenerateRow(this, &SAcousticsMaterialsTab::OnGenerateRowForMaterialList)
                .SelectionMode(ESelectionMode::SingleToggle)
                .OnSelectionChanged(this, &SAcousticsMaterialsTab::OnRowSelectionChanged)
                .HeaderRow
                (
                    SNew(SHeaderRow)
                    + SHeaderRow::Column(ColumnNameMaterial).DefaultLabel(column1HeaderText)
                        .FillWidth(0.4f) // Take 40% of the available space
                    + SHeaderRow::Column(ColumnNameAcoustics).DefaultLabel(column2HeaderText)
                        //.HAlignCell(HAlign_Center)
                        .FillWidth(0.4f) // Take 40% of the available space
                    + SHeaderRow::Column(ColumnNameAbsorption).DefaultLabel(column3HeaderText)
                        //.HAlignCell(HAlign_Right)
                        .FillWidth(0.2f) // Take 20% of the available space
                )
            ]
        ]
    ];
    // clang-format on
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAcousticsMaterialsTab::PublishMaterialLibrary()
{
    // Don't bother publishing if pre-bake computation is in progress
    if (AcousticsSharedState::GetSimulationConfiguration() &&
        AcousticsSharedState::GetSimulationConfiguration()->GetState() == SimulationConfigurationState::InProcess)
    {
        return;
    }

    TMap<FString, float> materialMap;
    for (TSharedPtr<MaterialItem>& item : m_Items)
    {
        materialMap.Add(item->UEMaterialName, item->Absorption);
    }

    TUniquePtr<AcousticsMaterialLibrary> materialLibrary = AcousticsMaterialLibrary::Create(materialMap);
    AcousticsSharedState::SetMaterialsLibrary(MoveTemp(materialLibrary));
}

void SAcousticsMaterialsTab::AddNewUEMaterialWithMigrationSupport(UMaterialInterface* curMaterial)
{
    if (curMaterial != nullptr)
    {
        // Instead of using unique ids to avoid duplicates, I simply check if the material item is in the list.
        for (const TSharedPtr<MaterialItem>& Item : m_Items)
        {
            if (Item->UEMaterialName == curMaterial->GetName())
            {
                return;
            }
        }

        // MIGRATION SUPPORT. We used to store material information in the material uasset
        // Now we store it in the config file
        // If the UAsset has our user data, read the material info out of there and then remove it
        // Otherwise, continue on to AddNewUEMaterial
        UAcousticsMaterialUserData* materialAssignment = curMaterial->GetAssetUserData<UAcousticsMaterialUserData>();
        if (materialAssignment != nullptr && !materialAssignment->AssignedMaterialName.IsEmpty())
        {
            // Update the materials list view
            m_Items.Add(MakeShared<MaterialItem>(MaterialItem(
                curMaterial->GetName(), materialAssignment->AssignedMaterialName, materialAssignment->Absorptivity)));
            curMaterial->RemoveUserDataOfClass(UAcousticsMaterialUserData::StaticClass());
            curMaterial->MarkPackageDirty();

            // Put the legacy data into the new config file
            FString tritonMaterialAsString = FString::Printf(
                TEXT("%s,%f"), *materialAssignment->AssignedMaterialName, materialAssignment->Absorptivity);

            FString ConfigFilePath;
            FConfigFile* BaseProjectAcousticsConfigFile;
            auto configLoaded = m_AcousticsEditMode->GetConfigFile(&BaseProjectAcousticsConfigFile, ConfigFilePath);
            if (configLoaded)
            {
                BaseProjectAcousticsConfigFile->SetString(
                    *c_ConfigSectionMaterials, *curMaterial->GetName(), *tritonMaterialAsString);
                BaseProjectAcousticsConfigFile->Write(ConfigFilePath);
            }
        }
        else
        {
            AddNewUEMaterial(curMaterial->GetName());
        }
    }
}

void SAcousticsMaterialsTab::UpdateUEMaterials()
{
    // Don't try to update if a pre-bake is running in the background - this will cause the UI to deadlock until it's
    // done.
    if (AcousticsSharedState::GetSimulationConfiguration() &&
        AcousticsSharedState::GetSimulationConfiguration()->GetState() == SimulationConfigurationState::InProcess)
    {
        return;
    }

    UWorld* currentWorld = GEditor->GetEditorWorldContext().World();

    m_Items.Empty();
    UMaterial* defaultMat = UMaterial::GetDefaultMaterial(MD_Surface);
    AddNewUEMaterial(defaultMat->GetName());

    for (FActorIterator ActorIter(currentWorld); ActorIter; ++ActorIter)
    {
        AActor* curActor = *ActorIter;

        // TODO - Add toggle to show all materials
        if (curActor == nullptr)
        {
            continue;
        }

        // Check for acoustic material override volumes here. They won't be tagged, but should always be included
        if (curActor->IsA<AAcousticsProbeVolume>())
        {
            AAcousticsProbeVolume* volume = Cast<AAcousticsProbeVolume>(curActor);
            if (volume->VolumeType == AcousticsVolumeType::MaterialOverride)
            {
                // Using the override material prefix.
                AddNewUEMaterial((AAcousticsProbeVolume::OverrideMaterialNamePrefix + volume->MaterialName));
            }
            // Check for acoustic remap volumes. They won't be tagged, but should always be included.
            // Add a material item for every remap defined in the volume.
            else if (volume->VolumeType == AcousticsVolumeType::MaterialRemap)
            {
                for (const TPair<FString, FString>& Remap : volume->MaterialRemapping)
                {
                    // Using the remap material prefix.
                    AddNewUEMaterial((AAcousticsProbeVolume::RemapMaterialNamePrefix + Remap.Value));
                }
            }
        }

        if (!curActor->Tags.Contains(c_AcousticsGeometryTag))
        {
            continue;
        }

        if (curActor->IsA<AStaticMeshActor>())
        {
            TArray<UMaterialInterface*> materials;

            if (curActor->IsA<AStaticMeshActor>())
            {
                const UStaticMeshComponent* curMeshComp = Cast<AStaticMeshActor>(curActor)->GetStaticMeshComponent();

                if (curMeshComp == nullptr)
                {
                    continue;
                }

                // This gets the override materials or the original static mesh materials as appropriate.
                materials = curMeshComp->GetMaterials();
            }

            for (UMaterialInterface* curMaterial : materials)
            {
                if (curMaterial != nullptr)
                {
                    AddNewUEMaterialWithMigrationSupport(curMaterial);
                }
            }
        }
        else if (curActor->IsA<ALandscapeProxy>()) // ALandscape derives from ALandscapeProxy
        {
            auto landscape = Cast<ALandscapeProxy>(curActor);
            UMaterialInterface* curMaterial = landscape->GetLandscapeMaterial();
            if (curMaterial != nullptr)
            {
                AddNewUEMaterialWithMigrationSupport(curMaterial);
            }
        }

        // Ignore all other actor types
    }

    // If the listview has already been created, then force it to update.
    if (m_ListView.Get() != nullptr)
    {
        m_ListView->RebuildList();
    }
}

// Instead of using unique ids to avoid duplicates, simply check if the material item has already been added.
void SAcousticsMaterialsTab::AddNewUEMaterial(FString materialName)
{
    for (const TSharedPtr<MaterialItem>& Item : m_Items)
    {
        if (Item->UEMaterialName == materialName)
        {
            return;
        }
    }

    {
        FConfigFile* BaseProjectAcousticsConfigFile;
        FString ConfigFilePath;
        auto configLoaded = m_AcousticsEditMode->GetConfigFile(&BaseProjectAcousticsConfigFile, ConfigFilePath);

        FString serializedTritonInfo;
        if (configLoaded &&
            BaseProjectAcousticsConfigFile->GetString(*c_ConfigSectionMaterials, *materialName, serializedTritonInfo))
        {
            TArray<FString> tritonInfoValues;
            if (serializedTritonInfo.ParseIntoArray(tritonInfoValues, TEXT(","), true) == 2)
            {
                TritonAcousticMaterial acousticMaterial;
                strncpy_s(
                    acousticMaterial.Name,
                    TRITON_MAX_NAME_LENGTH,
                    TCHAR_TO_ANSI(*(tritonInfoValues[0])),
                    tritonInfoValues[0].Len());
                acousticMaterial.Absorptivity = FCString::Atof(*tritonInfoValues[1]);
                m_Items.Add(MakeShared<MaterialItem>(
                    MaterialItem(materialName, acousticMaterial.Name, acousticMaterial.Absorptivity)));
            }
            else
            {
                UE_LOG(LogAcoustics, Error, TEXT("Deserialization error with UE material name %s."), *materialName);
                // If the serialization data is bad, clear it out
                // Remove the invalid serialized material data from the base ini file instead of the generated config
                // file.
                FConfigSection* MaterialsSection = BaseProjectAcousticsConfigFile->Find(c_ConfigSectionMaterials);
                if (MaterialsSection)
                {
                    MaterialsSection->Remove(*materialName);
                    if (MaterialsSection->Num() == 0)
                    {
                        BaseProjectAcousticsConfigFile->Remove(c_ConfigSectionMaterials);
                    }
                    BaseProjectAcousticsConfigFile->Dirty = true;
                }
                USourceControlHelpers::CheckOutOrAddFile(ConfigFilePath);
                BaseProjectAcousticsConfigFile->Write(ConfigFilePath);
            }
        }
        else
        {
            // Deserialization failed -- create a new material from the material name
            auto knownMaterialsLibrary = AcousticsSharedState::GetKnownMaterialsLibrary();
            TritonAcousticMaterial acousticMaterial;
            TritonMaterialCode materialCode;
            // No assignment was saved in the config for this material.
            // If the call to GuessMaterialInfoFromGeneralName fails, we just write an error to the log and skip it.
            if (knownMaterialsLibrary->GuessMaterialInfoFromGeneralName(materialName, acousticMaterial, materialCode))
            {
                m_Items.Add(MakeShared<MaterialItem>(
                    MaterialItem(materialName, acousticMaterial.Name, acousticMaterial.Absorptivity)));
            }
            else
            {
                UE_LOG(LogAcoustics, Error, TEXT("Attempt to match UE material name %s failed."), *materialName);
            }
        }
    }
}

void SAcousticsMaterialsTab::InitKnownMaterialsList()
{
    const AcousticsMaterialLibrary* knownMaterialsLibrary = AcousticsSharedState::GetKnownMaterialsLibrary();

    if (knownMaterialsLibrary == nullptr)
    {
        FString defaultMaterialsResourcePath = IPluginManager::Get().FindPlugin(c_PluginName)->GetBaseDir();
        defaultMaterialsResourcePath =
            FPaths::Combine(defaultMaterialsResourcePath, TEXT("Resources"), TEXT("DefaultMaterialProperties.json"));

        TUniquePtr<AcousticsMaterialLibrary> newLibrary =
            AcousticsMaterialLibrary::Create(defaultMaterialsResourcePath);

        knownMaterialsLibrary = newLibrary.Get();

        // Transfer ownership of the library to AcousticsSharedState
        AcousticsSharedState::SetKnownMaterialsLibrary(MoveTemp(newLibrary));
    }

    if (knownMaterialsLibrary == nullptr)
    {
        return;
    }

    knownMaterialsLibrary->GetKnownMaterials(m_KnownMaterials, m_KnownMaterialCodes);

    // Combobox requires all entries to be TSharedPtr, so we have to convert.
    for (int i = 0; i < m_KnownMaterials.Num(); i++)
    {
        // Don't add anything named 'default' to the dropdown list. We do that later.
        if (FString(m_KnownMaterials[i].Name).Compare(TEXT("Default"), ESearchCase::IgnoreCase) != 0)
        {
            m_ComboboxMaterialsList.Add(MakeShared<TritonAcousticMaterial>(m_KnownMaterials[i]));
        }
    }

    // Sort combobox list by name
    m_ComboboxMaterialsList.Sort(
        [](const TSharedPtr<TritonAcousticMaterial>& left, const TSharedPtr<TritonAcousticMaterial>& right) -> bool {
            return FString(left->Name).Compare(FString(right->Name), ESearchCase::IgnoreCase) < 0;
        });

    float defaultAbsorptivity = knownMaterialsLibrary->GetMaterialInfo(TRITON_DEFAULT_WALL_CODE).Absorptivity;

    TritonAcousticMaterial customEntry{"Custom", 0.3f};
    TritonAcousticMaterial defaultEntry{"Default", defaultAbsorptivity};

    // Insert Default and Custom entries.
    m_ComboboxMaterialsList.Insert(MakeShared<TritonAcousticMaterial>(defaultEntry), 0);
    m_ComboboxMaterialsList.Insert(MakeShared<TritonAcousticMaterial>(customEntry), 1);
}

TSharedRef<ITableRow> SAcousticsMaterialsTab::OnGenerateRowForMaterialList(
    TSharedPtr<MaterialItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(FMaterialRow, OwnerTable, InItem, m_ComboboxMaterialsList);
}

void SAcousticsMaterialsTab::OnRowSelectionChanged(TSharedPtr<MaterialItem> InItem, ESelectInfo::Type SelectInfo)
{
    // Select all the actors in the world that use the selected material.

    UWorld* currentWorld = GEditor->GetEditorWorldContext().World();

    GEditor->SelectNone(false, true, false);

    if (!InItem.IsValid())
    {
        GEditor->NoteSelectionChange();
        return;
    }

    for (FActorIterator ActorIter(currentWorld); ActorIter; ++ActorIter)
    {
        AActor* curActor = *ActorIter;

        if (curActor->IsA<AStaticMeshActor>())
        {
            TArray<UMaterialInterface*> materials;
            const UStaticMeshComponent* curMeshComp = Cast<AStaticMeshActor>(curActor)->GetStaticMeshComponent();

            if (curMeshComp == nullptr)
            {
                continue;
            }

            // This gets the override materials or the original static mesh materials as appropriate.
            materials = curMeshComp->GetMaterials();

            for (UMaterialInterface* curMaterial : materials)
            {
                if (curMaterial->GetName() == InItem->UEMaterialName)
                {
                    GEditor->SelectActor(curActor, true, false, true, false);
                    break;
                }
            }
        }
        else if (curActor->IsA<ALandscapeProxy>()) // ALandscape derives from ALandscapeProxy
        {
            UMaterialInterface* curMaterial = Cast<ALandscapeProxy>(curActor)->GetLandscapeMaterial();

            if (curMaterial->GetName() == InItem->UEMaterialName)
            {
                GEditor->SelectActor(curActor, true, false, true, false);
            }
        }
    }

    GEditor->NoteSelectionChange();
}