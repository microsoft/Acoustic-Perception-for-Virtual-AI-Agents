// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once
#include "SAcousticsEdit.h"
#include "Widgets/SCompoundWidget.h"
#include "Runtime/Core/Public/Containers/Array.h"
#include "AcousticsMesh.h"
#include "AcousticsProbesTab.generated.h"

UENUM()
enum class FResolution : uint8
{
    Coarse = 0,
    Fine
};

static TArray<TSharedPtr<FString>> g_ResolutionNames = {MakeShareable(new FString(TEXT("Coarse"))),
                                                        MakeShareable(new FString(TEXT("Fine")))};

static TArray<float> g_ResolutionFrequencies = {250, 500};

static inline FResolution LabelToResolution(TSharedPtr<FString> label)
{
    return FString(TEXT("Coarse")).Compare(*label) == 0 ? FResolution::Coarse : FResolution::Fine;
}

static inline FResolution FrequencyToResolution(float frequency)
{
    return (frequency == 250) ? FResolution::Coarse : FResolution::Fine;
}

class SAcousticsProbesTab : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SAcousticsProbesTab)
    {
    }
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, SAcousticsEdit* ownerEdit);

private:
    FText GetCalculateClearText() const;
    FText GetCalculateClearTooltipText() const;
    FReply OnCalculateClearButton();
    // Added functions associated with the check out button for the config and vox files.
    FReply OnCheckOutFilesButton();
    void CheckOutVoxAndConfigFile();
    bool CanCheckOutFiles() const;

    FText GetCurrentResolutionLabel() const;
    TSharedRef<SWidget> MakeResolutionOptionsWidget(TSharedPtr<FString> inString);
    void OnResolutionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    FText GetPrefixText() const;
    void OnPrefixTextChange(const FText& NewText, ETextCommit::Type CommitInfo);
    FText GetDataFolderPath() const;
    FReply OnAcousticsDataFolderButtonClick();
    void ComputePrebake();
    void AddStaticMeshToAcousticMesh(
        AcousticMesh* acousticMesh, AActor* actor, const UStaticMesh* mesh,
        const TArray<UMaterialInterface*>& materials, MeshType type, TArray<uint32>& materialIDsNotFound);
    void AddLandscapeToAcousticMesh(
        AcousticMesh* acousticMesh, class ALandscapeProxy* actor, MeshType type, TArray<uint32>& materialIDsNotFound);
    void AddVolumeToAcousticMesh(
        AcousticMesh* acousticMesh, class AAcousticsProbeVolume* Actor, TArray<uint32>& materialIDsNotFound);
    void AddPinnedProbeToAcousticMesh(AcousticMesh* acousticMesh, const FVector& probeLocation);

    void AddNavmeshToAcousticMesh(
        AcousticMesh* acousticMesh, class ARecastNavMesh* navActor, TArray<UMaterialInterface*> materials,
        TArray<uint32>& materialIDsNotFound);
    bool ShouldEnableForProcessing() const;
    TOptional<float> GetProgressBarPercent() const;
    EVisibility GetProgressBarVisibility() const;

    static bool ComputePrebakeCallback(char* message, int progress);
    static void ResetPrebakeCalculationState();
    static bool IsOverlapped(
        const class AAcousticsProbeVolume* ProbeVolume, const ATKVectorF& Vertex1, const ATKVectorF& Vertex2,
        const ATKVectorF& Vertex3);

private:
    TSharedPtr<FString> m_CurrentResolution;
    FString m_AcousticsDataFolderPath;
    TSharedPtr<class SEditableTextBox> m_PrefixTextBox;
    FString m_Prefix;
    SAcousticsEdit* m_OwnerEdit;
    static FString m_CurrentStatus;
    static float m_CurrentProgress;
    static bool m_CancelRequest;

    TArray<class AAcousticsProbeVolume*> m_MaterialOverrideVolumes;
    TArray<class AAcousticsProbeVolume*> m_MaterialRemapVolumes;
};
