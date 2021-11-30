// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//   Created from SFoliageEdit class code

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Editor/EditorStyle/Public/EditorStyleSet.h"
#include "AcousticsEdMode.h"

class SAcousticsPalette;
class UAcousticsType;
struct FAcousticsMeshUIInfo;
enum class ECheckBoxState : uint8;

typedef TSharedPtr<FAcousticsMeshUIInfo> FAcousticsMeshUIInfoPtr; // should match typedef in AcousticsEdMode.h

struct FAcousticsEditSharedProperties
{
    static inline FSlateFontInfo GetStandardFont()
    {
        return FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont"));
    }

    static const FMargin StandardPadding;
    static const FMargin DoubleBottomPadding;
    static const FMargin StandardLeftPadding;
    static const FMargin StandardRightPadding;
    static const FMargin StandardTextMargin;
};

class SAcousticsEdit : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SAcousticsEdit)
    {
    }
    SLATE_END_ARGS()

public:
    /** SCompoundWidget functions */
    void Construct(const FArguments& InArgs);

    /** Does a full refresh on the list. */
    void RefreshFullList();

    /** Will return the status of editing mode */
    // bool IsAcousticsEditorEnabled() const;

    /** Sets the current error text */
    void SetError(FString errorText);

    /** Helper to get help text panel for the tabs */
    static TSharedRef<SWidget> MakeHelpTextWidget(const FString& title, const FString& text);

private:
    /** Creates the toolbar. */
    TSharedRef<SWidget> BuildToolBar();

    /** Checks if the tab mode is Object Mark. */
    bool IsObjectMarkTab() const;

    /** Checks if the tab mode is Materials. */
    bool IsMaterialsTab() const;

    /** Checks if the tab mode is Probes. */
    bool IsProbesTab() const;

    /** Checks if the tab mode is Bake. */
    bool IsBakeTab() const;

    FText GetActiveTabName() const;

    /** Helper function to build the individual tab UIs. */
    TSharedRef<SWidget> BuildObjectTagTab();
    TSharedRef<SWidget> BuildMaterialsTab();
    TSharedRef<SWidget> BuildProbesTab();
    TSharedRef<SWidget> BuildBakeTab();

    EVisibility GetObjectTagTabVisibility() const
    {
        return (IsObjectMarkTab() ? EVisibility::Visible : EVisibility::Collapsed);
    }

    EVisibility GetMaterialsTabVisibility() const
    {
        return (IsMaterialsTab() ? EVisibility::Visible : EVisibility::Collapsed);
    }

    EVisibility GetProbesTabVisibility() const
    {
        return (IsProbesTab() ? EVisibility::Visible : EVisibility::Collapsed);
    }

    EVisibility GetBakeTabVisibility() const
    {
        return (IsBakeTab() ? EVisibility::Visible : EVisibility::Collapsed);
    }

private:
    /** Tooltip text for 'Instance Count" column */
    // FText GetTotalInstanceCountTooltipText() const;

    /** Handler to trigger a refresh of the details view when the active tab changes */
    // void HandleOnTabChanged();

private:
    /** Complete list of available materials   */
    TSharedPtr<class MaterialsLibrary> m_MaterialsLibrary;

    /** Current error message */
    TSharedPtr<class SErrorText> m_ErrorText;

    /** Pointer to the acoustics edit mode. */

    FAcousticsEdMode* m_AcousticsEditMode;
};
