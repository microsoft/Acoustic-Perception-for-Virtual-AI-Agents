// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "AcousticsBakeTab.h"
#include "SAcousticsEdit.h"
#include "AcousticsSharedState.h"
#include "AcousticsEdMode.h"
#include "Fonts/SlateFontInfo.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSeparator.h"
#include "Misc/Timespan.h"
#include "Misc/Paths.h"
#include "AcousticsAzureCredentialsPanel.h"
#include "AcousticsPythonBridge.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SButton.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "Misc/MessageDialog.h"
#include "SourceControlOperations.h"
#include "HAL/PlatformFilemanager.h"

#define LOCTEXT_NAMESPACE "SAcousticsBakeTab"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAcousticsBakeTab::Construct(const FArguments& InArgs, SAcousticsEdit* ownerEdit)
{
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

    m_OwnerEdit = ownerEdit;

    const FString helpTextTitle = TEXT("Step Four");
    const FString helpText = TEXT("After completing the previous steps, submit the job for baking in the cloud. "
                                  "Make sure you have created your Azure Batch and Storage accounts.");

    // clang-format off
    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FAcousticsEditSharedProperties::StandardPadding)
        [
            SAcousticsEdit::MakeHelpTextWidget(helpTextTitle, helpText)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FAcousticsEditSharedProperties::StandardPadding)
        [
            SNew(SAcousticsAzureCredentialsPanel)
        ]

        // Compute pool configuration
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FAcousticsEditSharedProperties::StandardPadding)
        [
            SAssignNew(m_ComputePoolPanel, SAcousticsComputePoolConfigurationPanel)
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
        .Padding(FAcousticsEditSharedProperties::StandardPadding)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            .AutoWidth()
            [
                SNew(SButton)
                .IsEnabled(this, &SAcousticsBakeTab::ShouldEnableSubmitCancel)
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                .DesiredSizeScale(FVector2D(3.0f, 1.f))
                .Text(this, &SAcousticsBakeTab::GetSubmitCancelText)
                .OnClicked(this, &SAcousticsBakeTab::OnSubmitCancelButton)
                .ToolTipText(this, &SAcousticsBakeTab::GetSubmitCancelTooltipText)
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FAcousticsEditSharedProperties::StandardPadding)
        [
            SNew(STextBlock)
            .Text(this, &SAcousticsBakeTab::GetProbeCountText)
            .Visibility_Lambda([this]() { return HaveValidSimulationConfig() ? EVisibility::Visible : EVisibility::Collapsed; })
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FAcousticsEditSharedProperties::StandardPadding)
        [
            SNew(STextBlock)
            .Text(this, &SAcousticsBakeTab::GetCurrentStatus)
            .AutoWrapText(true)
        ]
    ];
    // clang-format on
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAcousticsBakeTab::Refresh()
{
    m_ComputePoolPanel->Refresh();
}

bool SAcousticsBakeTab::ShouldEnableSubmitCancel() const
{
    const auto& config = AcousticsSharedState::GetActiveJobInfo();
    // Submit is pending
    if (config.submit_pending)
    {
        return false;
    }

    // Either have a valid creds and config ready for submission or tracking an active bake job
    return (HaveValidAzureCredentials() && HaveValidSimulationConfig()) || config.job_id.IsEmpty() == false;
}

bool SAcousticsBakeTab::HaveValidAzureCredentials() const
{
    const auto& creds = AcousticsSharedState::GetAzureCredentials();
    if (creds.batch_url.IsEmpty() || creds.batch_name.IsEmpty() || creds.batch_key.IsEmpty() ||
        creds.storage_name.IsEmpty() || creds.storage_key.IsEmpty() || creds.toolset_version.IsEmpty())
    {
        return false;
    }
    return true;
}

bool SAcousticsBakeTab::HaveValidSimulationConfig() const
{
    return AcousticsSharedState::GetSimulationConfiguration() != nullptr &&
           AcousticsSharedState::GetSimulationConfiguration()->IsReady();
}

FText SAcousticsBakeTab::GetSubmitCancelText() const
{
    const auto& info = AcousticsSharedState::GetActiveJobInfo();

    if (info.job_id.IsEmpty())
    {
        return FText::FromString(TEXT("Bake"));
    }
    else
    {
        return FText::FromString(TEXT("Cancel"));
    }
}

FText SAcousticsBakeTab::GetSubmitCancelTooltipText() const
{
    const auto& info = AcousticsSharedState::GetActiveJobInfo();

    if (info.job_id.IsEmpty())
    {
        return FText::FromString(TEXT("Submit to Azure Batch for processing"));
    }
    else
    {
        return FText::FromString(TEXT("Cancel currently active Azure Batch processing"));
    }
}

FReply SAcousticsBakeTab::OnSubmitCancelButton()
{
    const auto& info = AcousticsSharedState::GetActiveJobInfo();

    if (info.job_id.IsEmpty())
    {
        if (AcousticsSharedState::IsAceFileReadOnly())
        {
            auto message = FString(TEXT("Please provide write access to  " + AcousticsSharedState::GetAceFilepath()));
            FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(message));
            return FReply::Handled();
        }
        else
        {
            auto consent = EAppReturnType::Ok;
            if (FPaths::FileExists(AcousticsSharedState::GetAceFilepath()))
            {
                auto message = FString(TEXT(
                    "Current results file " + AcousticsSharedState::GetAceFilepath() +
                    " will be replaced when simulation completes. Continue?"));
                consent = FMessageDialog::Open(EAppMsgType::OkCancel, FText::FromString(message));
            }

            if (consent == EAppReturnType::Ok)
            {
                SubmitForProcessing();
            }
            return FReply::Handled();
        }
    }
    else
    {
        return CancelJobProcessing();
    }
}

FReply SAcousticsBakeTab::SubmitForProcessing()
{
    AcousticsSharedState::SubmitForProcessing();
    return FReply::Handled();
}

FReply SAcousticsBakeTab::CancelJobProcessing()
{
    AcousticsSharedState::CancelProcessing();
    return FReply::Handled();
}

FText SAcousticsBakeTab::GetProbeCountText() const
{
    FText message;
    auto config = AcousticsSharedState::GetSimulationConfiguration();
    if (config)
    {
        message = FText::FromString(FString::Printf(TEXT("Probe Count: %d"), config->GetProbeCount()));
    }
    return message;
}

FText SAcousticsBakeTab::GetCurrentStatus() const
{
    const auto& info = AcousticsSharedState::GetActiveJobInfo();

    FString jobInfo;
    FString header = TEXT("Status: ");
    if (info.job_id.IsEmpty() && info.submit_pending == false)
    {
        m_Status = TEXT("");
        // Inform user about the Bake button state
        if (ShouldEnableSubmitCancel())
        {
            m_Status = TEXT("Ready for processing\n");
        }
        else
        {
            if (HaveValidSimulationConfig() == false)
            {
                m_Status = TEXT(
                    "Please generate a simulation configuration using the Probes tab to enable acoustics baking\n");
            }
            else if (HaveValidAzureCredentials() == false)
            {
                m_Status = TEXT("Please provide Azure account credentials to enable acoustics baking\n");
            }
        }

        // Check for an existing ACE file and inform about location
        auto aceFile = AcousticsSharedState::GetAceFilepath();
        if (FPaths::FileExists(aceFile))
        {
            m_Status = m_Status + TEXT("\nFound existing simulation results in ") + aceFile + "\n";

            // Track the total time taken for a bake.
            if (AcousticsSharedState::BakeStartTime.GetTicks() != 0)
            {
                if (AcousticsSharedState::BakeEndTime.GetTicks() == 0)
                {
                    AcousticsSharedState::BakeEndTime = FDateTime::Now();

                    // Check out the generated ace and uasset file.
                    // reaches here if baking process just ended.
                    TArray<FString> FilesToCheckOut;
                    FilesToCheckOut.Add(aceFile); // ace file
                    FString UassetFilePath = aceFile;
                    UassetFilePath.RemoveFromEnd("ace");
                    UassetFilePath.Append("uasset");
                    FilesToCheckOut.Add(UassetFilePath); // uasset file

                    USourceControlHelpers::CheckOutFiles(FilesToCheckOut);

                    FString AceFileBackup = AcousticsSharedState::GetAceFileBackupPath();
                    if (FPaths::FileExists(AceFileBackup))
                    {
                        FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*AceFileBackup);
                    }
                }
                if (AcousticsSharedState::BakeEndTime.GetTicks() != 0)
                {
                    m_Status.Append(FString::Printf(
                        TEXT("Started: %s\nEnded: %s\nTotal Duration: %s\n"),
                        *AcousticsSharedState::BakeStartTime.ToString(),
                        *AcousticsSharedState::BakeEndTime.ToString(),
                        *(AcousticsSharedState::BakeEndTime - AcousticsSharedState::BakeStartTime).ToString()));
                }
            }
        }
    }
    else
    {
        // Check if it's time to query for status
        auto elapsed = FDateTime::Now() - m_LastStatusCheckTime;
        if (elapsed > FTimespan::FromSeconds(30))
        {
            auto status = AcousticsSharedState::GetCurrentStatus();
            m_Status = status.message;
            if (status.succeeded)
            {
                m_OwnerEdit->SetError(TEXT(""));
            }
            else
            {
                UE_LOG(LogAcoustics, Error, TEXT("%s"), *m_Status);
                m_OwnerEdit->SetError(m_Status);
            }
            m_LastStatusCheckTime = FDateTime::Now();
        }

        if (info.submit_pending == false)
        {
            jobInfo = TEXT("Job ID: ") + info.job_id + TEXT("\n");
            jobInfo = jobInfo + TEXT("Submit Time: ") + info.submit_time;
        }
    }
    return FText::FromString(header + m_Status + TEXT("\n\n") + jobInfo);
}

#undef LOCTEXT_NAMESPACE