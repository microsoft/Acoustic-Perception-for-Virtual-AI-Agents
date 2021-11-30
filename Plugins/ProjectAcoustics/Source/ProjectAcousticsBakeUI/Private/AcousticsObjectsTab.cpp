// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "AcousticsObjectsTab.h"
#include "AcousticsSharedState.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckbox.h"
#include "Widgets/Layout/SHeader.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SToolTip.h"
#include "SlateOptMacros.h"

#include "EditorModeManager.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "SAcousticsObjectsTab"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAcousticsObjectsTab::Construct(const FArguments& InArgs, SAcousticsEdit* ownerEdit)
{
    m_Owner = ownerEdit;
    m_AcousticsEditMode =
        static_cast<FAcousticsEdMode*>(GLevelEditorModeTools().GetActiveMode(FAcousticsEdMode::EM_AcousticsEdModeId));
    FSlateFontInfo StandardFont = FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont"));
    FMargin StandardPadding(6.f, 3.f);

    const FString helpTextTitle = TEXT("Step One");
    const FString helpText =
        TEXT("Select geometry and navigation objects in the scene that impact the acoustics simulation.");

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
        .Padding(StandardPadding)
        [
            SNew(SHeader)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("SelectHeader", "Bulk Selection"))
                .Font(StandardFont)
            ]
        ]

        + SVerticalBox::Slot()
        .Padding(StandardPadding)
        .AutoHeight()
        [
            SNew(SHorizontalBox)

            +SHorizontalBox::Slot()
            .VAlign(VAlign_Center)
            .Padding(StandardPadding)
            [
                SNew(SWrapBox)
                .UseAllottedWidth(true)
                .InnerSlotPadding({ 6, 5 })

                +SWrapBox::Slot()
                [
                    SNew(SBox)
                    .MinDesiredWidth(91)
                    [
                        SNew(SCheckBox)
                        .IsEnabled_Lambda([]() { return (AcousticsSharedState::GetSimulationConfiguration() == nullptr); })
                        .OnCheckStateChanged(this, &SAcousticsObjectsTab::OnCheckStateChanged_StaticMesh)
                        .IsChecked(this, &SAcousticsObjectsTab::GetCheckState_StaticMesh)
                        .ToolTipText(LOCTEXT("StaticMeshTooltip", "Select all static meshes marked as static (not moveable)"))
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("StaticMesh", "Static Meshes"))
                            .Font(StandardFont)
                        ]
                    ]
                ]

                + SWrapBox::Slot()
                [
                    SNew(SBox)
                    .MinDesiredWidth(91)
                    [
                        SNew(SCheckBox)
                        .IsEnabled_Lambda([]() { return (AcousticsSharedState::GetSimulationConfiguration() == nullptr); })
                        .OnCheckStateChanged(this, &SAcousticsObjectsTab::OnCheckStateChanged_NavMesh)
                        .IsChecked(this, &SAcousticsObjectsTab::GetCheckState_NavMesh)
                        .ToolTipText(LOCTEXT("NavMeshTooltip", "Select all Navigation Meshes"))
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("NavMesh", "Navigation Meshes"))
                            .Font(StandardFont)
                        ]
                    ]
                ]

                + SWrapBox::Slot()
                [
                    SNew(SBox)
                    .MinDesiredWidth(91)
                    [
                        SNew(SCheckBox)
                        .IsEnabled_Lambda([]() { return (AcousticsSharedState::GetSimulationConfiguration() == nullptr); })
                        .OnCheckStateChanged(this, &SAcousticsObjectsTab::OnCheckStateChanged_Landscape)
                        .IsChecked(this, &SAcousticsObjectsTab::GetCheckState_Landscape)
                        .ToolTipText(LOCTEXT("LandscapeTooltip", "Select all Landscapes"))
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("Landscapes", "Landscapes"))
                            .Font(StandardFont)
                        ]
                    ]
                ]
            ]
        ]

        // Selection Buttons
        + SVerticalBox::Slot()
        .Padding(StandardPadding)
        .AutoHeight()
        [
            SNew(SWrapBox)
            .UseAllottedWidth(true)

            // Select all instances
            +SWrapBox::Slot()
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            [
                SNew(SBox)
                .MinDesiredWidth(60.f)
                .HeightOverride(25.f)
                [
                    SNew(SButton)
                    .IsEnabled_Lambda([]() { return (AcousticsSharedState::GetSimulationConfiguration() == nullptr); })
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    .OnClicked(this, &SAcousticsObjectsTab::OnSelectObjects)
                    .Text(LOCTEXT("Select", "Select"))
                    .ToolTipText(LOCTEXT("Select_Tooltip", "Selects all objects matching the filter"))
                ]
            ]

            // Deselect everything
            +SWrapBox::Slot()
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            [
                SNew(SBox)
                .MinDesiredWidth(90.f)
                .HeightOverride(25.f)
                [
                    SNew(SButton)
                    .IsEnabled_Lambda([]() { return (AcousticsSharedState::GetSimulationConfiguration() == nullptr); })
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    .OnClicked(this, &SAcousticsObjectsTab::OnUnselectObjects)
                    .Text(LOCTEXT("Deselect", "Deselect all"))
                    .ToolTipText(LOCTEXT("Unselect_Tooltip", "Deselect all objects"))
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(StandardPadding)
        [
            SNew(SHeader)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("TagHeader", "Tagging"))
                .Font(StandardFont)
            ]
        ]

        // Add tag selectors
        + SVerticalBox::Slot()
        .Padding(StandardPadding)
        .AutoHeight()
        [
            // Geometry Tag
            SNew(SWrapBox)
            .UseAllottedWidth(true)
            +SWrapBox::Slot()
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            [
                SNew(SCheckBox)
                .Style(FEditorStyle::Get(), "RadioButton")
                .IsEnabled_Lambda([]() { return (AcousticsSharedState::GetSimulationConfiguration() == nullptr); })
                .Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
                .IsChecked(this, &SAcousticsObjectsTab::IsAcousticsRadioButtonChecked)
                .OnCheckStateChanged(this, &SAcousticsObjectsTab::OnAcousticsRadioButtonChanged)
                .ToolTipText(LOCTEXT("GeometryTag_Tooltip", "Add the Geometry tag to any objects that will have an effect on the sound (walls, floors, etc)."))
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("GeometryTag", "Geometry"))
                    .Font(StandardFont)
                ]
            ]

            // Navigation Tag
            +SWrapBox::Slot()
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            [
                SNew(SCheckBox)
                .Style(FEditorStyle::Get(), "RadioButton")
                .IsEnabled_Lambda([]() { return (AcousticsSharedState::GetSimulationConfiguration() == nullptr); })
                .Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
                .IsChecked(this, &SAcousticsObjectsTab::IsNavigationRadioButtonChecked)
                .OnCheckStateChanged(this, &SAcousticsObjectsTab::OnNavigationRadioButtonChanged)
                .ToolTipText(LOCTEXT("NavigationTag_Tooltip", "Add Navigation tag to meshes that define where the player can navigate. This informs where listener probes are placed for wave physics simulation. At least one object must have this tag."))
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("NavigationTag", "Navigation"))
                    .Font(StandardFont)
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .Padding(StandardPadding)
        .AutoHeight()
        [
            // Add Tag
            SNew(SWrapBox)
            .UseAllottedWidth(true)
            +SWrapBox::Slot()
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            [
                SNew(SBox)
                .MinDesiredWidth(60.f)
                .HeightOverride(25.f)
                [
                    SNew(SButton)
                    .IsEnabled_Lambda([]() { return (AcousticsSharedState::GetSimulationConfiguration() == nullptr); })
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    .OnClicked(this, &SAcousticsObjectsTab::OnAddTag)
                    .Text(LOCTEXT("Tag", "Tag"))
                    .ToolTipText(LOCTEXT("AddTag_Tooltip", "Add Tag to all selected objects"))
                ]
            ]

            // Clear Tag
            +SWrapBox::Slot()
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            [
                SNew(SBox)
                .MinDesiredWidth(60.f)
                .HeightOverride(25.f)
                [
                    SNew(SButton)
                    .IsEnabled_Lambda([]() { return (AcousticsSharedState::GetSimulationConfiguration() == nullptr); })
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    .OnClicked(this, &SAcousticsObjectsTab::OnClearTag)
                    .Text(LOCTEXT("Untag", "Untag"))
                    .ToolTipText(LOCTEXT("ClearTag_Tooltip", "Remove Tag from all selected objects"))
                ]
            ]

            // Select All Tagged items
            + SWrapBox::Slot()
            .Padding(FAcousticsEditSharedProperties::StandardPadding)
            [
                SNew(SBox)
                .MinDesiredWidth(60.f)
                .HeightOverride(25.f)
                [
                    SNew(SButton)
                    .IsEnabled_Lambda([]() { return (AcousticsSharedState::GetSimulationConfiguration() == nullptr); })
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    .OnClicked(this, &SAcousticsObjectsTab::OnSelectAllTag)
                    .Text(LOCTEXT("SelectTagged", "Select Tagged"))
                    .ToolTipText(LOCTEXT("SelectAll_Tooltip", "Select all objects with current tag"))
                ]
            ]
        ]

        // Display Tagged Stats
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FAcousticsEditSharedProperties::StandardPadding)
        [
            SNew(STextBlock)
            .AutoWrapText(true)
            .Text_Lambda([this]() { return FText::FromString(m_NumGeo); })
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FAcousticsEditSharedProperties::StandardPadding)
        [
            SNew(STextBlock)
            .AutoWrapText(true)
            .Text_Lambda([this]() { return FText::FromString(m_NumNav); })
        ]
    ];
    // clang-format on
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAcousticsObjectsTab::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    // For the objects tab, every frame we should count up the number of tagged items and update our counts
    // This is because users can change the tags outside of our UI. This method makes sure the displayed
    // tagged counts are always accurate

    int geo = 0;
    int nav = 0;
    for (TActorIterator<AActor> ActorItr(GEditor->GetEditorWorldContext().World()); ActorItr; ++ActorItr)
    {
        AActor* actor = *ActorItr;
        if (actor->ActorHasTag(c_AcousticsGeometryTag))
        {
            geo += 1;
        }
        if (actor->ActorHasTag(c_AcousticsNavigationTag))
        {
            nav += 1;
        }
    }
    m_NumGeo = FString::Printf(TEXT("Objects tagged for Geometry: %d"), geo);
    m_NumNav = FString::Printf(TEXT("Objects tagged for Navigation: %d"), nav);
}

void SAcousticsObjectsTab::OnAcousticsRadioButtonChanged(ECheckBoxState inState)
{
    bool isChecked = (inState == ECheckBoxState::Checked ? true : false);
    if (isChecked)
    {
        m_AcousticsEditMode->AcousticsUISettings.ObjectsTabSettings.IsAcousticsRadioButtonChecked = true;
        m_AcousticsEditMode->AcousticsUISettings.ObjectsTabSettings.IsNavigationRadioButtonChecked = false;
    }
}

ECheckBoxState SAcousticsObjectsTab::IsAcousticsRadioButtonChecked() const
{
    return m_AcousticsEditMode->AcousticsUISettings.ObjectsTabSettings.IsAcousticsRadioButtonChecked
               ? ECheckBoxState::Checked
               : ECheckBoxState::Unchecked;
}

void SAcousticsObjectsTab::OnNavigationRadioButtonChanged(ECheckBoxState inState)
{
    bool isChecked = (inState == ECheckBoxState::Checked ? true : false);
    if (isChecked)
    {
        m_AcousticsEditMode->AcousticsUISettings.ObjectsTabSettings.IsNavigationRadioButtonChecked = true;
        m_AcousticsEditMode->AcousticsUISettings.ObjectsTabSettings.IsAcousticsRadioButtonChecked = false;
    }
}

ECheckBoxState SAcousticsObjectsTab::IsNavigationRadioButtonChecked() const
{
    return m_AcousticsEditMode->AcousticsUISettings.ObjectsTabSettings.IsNavigationRadioButtonChecked
               ? ECheckBoxState::Checked
               : ECheckBoxState::Unchecked;
}

void SAcousticsObjectsTab::OnCheckStateChanged_StaticMesh(ECheckBoxState InState)
{
    m_AcousticsEditMode->AcousticsUISettings.ObjectsTabSettings.IsStaticMeshChecked =
        (InState == ECheckBoxState::Checked ? true : false);
}

ECheckBoxState SAcousticsObjectsTab::GetCheckState_StaticMesh() const
{
    return m_AcousticsEditMode->AcousticsUISettings.ObjectsTabSettings.IsStaticMeshChecked ? ECheckBoxState::Checked
                                                                                           : ECheckBoxState::Unchecked;
}

void SAcousticsObjectsTab::OnCheckStateChanged_NavMesh(ECheckBoxState InState)
{
    m_AcousticsEditMode->AcousticsUISettings.ObjectsTabSettings.IsNavMeshChecked =
        (InState == ECheckBoxState::Checked ? true : false);
}

ECheckBoxState SAcousticsObjectsTab::GetCheckState_NavMesh() const
{
    return m_AcousticsEditMode->AcousticsUISettings.ObjectsTabSettings.IsNavMeshChecked ? ECheckBoxState::Checked
                                                                                        : ECheckBoxState::Unchecked;
}

void SAcousticsObjectsTab::OnCheckStateChanged_Landscape(ECheckBoxState InState)
{
    m_AcousticsEditMode->AcousticsUISettings.ObjectsTabSettings.IsLandscapeChecked =
        (InState == ECheckBoxState::Checked ? true : false);
}

ECheckBoxState SAcousticsObjectsTab::GetCheckState_Landscape() const
{
    return m_AcousticsEditMode->AcousticsUISettings.ObjectsTabSettings.IsLandscapeChecked ? ECheckBoxState::Checked
                                                                                          : ECheckBoxState::Unchecked;
}

FReply SAcousticsObjectsTab::OnSelectObjects()
{
    m_AcousticsEditMode->SelectObjects();
    return FReply::Handled();
}
FReply SAcousticsObjectsTab::OnUnselectObjects()
{
    GEditor->SelectNone(true, true, false);
    return FReply::Handled();
}

FReply SAcousticsObjectsTab::OnAddTag()
{
    auto success = false;
    if (m_AcousticsEditMode->AcousticsUISettings.ObjectsTabSettings.IsNavigationRadioButtonChecked)
    {
        success = m_AcousticsEditMode->TagNavigation(true);
        if (!success)
        {
            m_Owner->SetError(TEXT("Failed to tag one or more objects for Navigation"));
        }
    }
    else
    {
        success = m_AcousticsEditMode->TagGeometry(true);
        if (!success)
        {
            m_Owner->SetError(TEXT("Failed to tag one or more objects for Geometry"));
        }
    }
    if (success)
    {
        m_Owner->SetError(TEXT(""));
    }
    return FReply::Handled();
}

FReply SAcousticsObjectsTab::OnClearTag()
{
    if (m_AcousticsEditMode->AcousticsUISettings.ObjectsTabSettings.IsNavigationRadioButtonChecked)
    {
        m_AcousticsEditMode->TagNavigation(false);
    }
    else
    {
        m_AcousticsEditMode->TagGeometry(false);
    }
    return FReply::Handled();
}

FReply SAcousticsObjectsTab::OnSelectAllTag()
{
    auto tag = c_AcousticsGeometryTag;
    if (m_AcousticsEditMode->AcousticsUISettings.ObjectsTabSettings.IsNavigationRadioButtonChecked)
    {
        tag = c_AcousticsNavigationTag;
    }

    GEditor->SelectNone(true, true, false);
    for (TActorIterator<AActor> ActorItr(GEditor->GetEditorWorldContext().World()); ActorItr; ++ActorItr)
    {
        AActor* actor = *ActorItr;
        if (actor->ActorHasTag(tag))
        {
            GEditor->SelectActor(actor, true, false, true, false);
        }
    }

    GEditor->NoteSelectionChange();
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE