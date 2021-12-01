// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "AcousticsDataFactory.h"
#include "AcousticsData.h"
#include "Misc/Paths.h"
#include "AssetRegistryModule.h"
#include "PackageTools.h"
#include "AcousticsEdMode.h"
#include "Logging/LogMacros.h"

UAcousticsDataFactory::UAcousticsDataFactory(const class FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Property initialization
    SupportedClass = UAcousticsData::StaticClass();
    Formats.Add(TEXT("ace;Project Acoustics Data"));
    bCreateNew = true;
    bEditorImport = true;
    bEditAfterNew = true;
    bText = false;
    // Turn off auto-reimport for this factory.
    ImportPriority = -1;
}

UObject* UAcousticsDataFactory::FactoryCreateNew(
    UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
    UAcousticsData* data = NewObject<UAcousticsData>(InParent, Name, Flags);
    return data;
}

bool UAcousticsDataFactory::FactoryCanImport(const FString& Filename)
{
    // check extension
    if (FPaths::GetExtension(Filename) == TEXT("ace"))
    {
        return true;
    }

    return false;
}

UObject* UAcousticsDataFactory::ImportFromFile(const FString& aceFilepath)
{
    auto name = FPaths::GetBaseFilename(aceFilepath);
    auto packageName = FString(TEXT("/Game/Acoustics/")) + name;
    auto packageFilename =
        FPackageName::LongPackageNameToFilename(packageName, FPackageName::GetAssetPackageExtension());

    // Does the UAsset already exist?
    auto uasset = LoadObject<UAcousticsData>(
        nullptr, *packageName, *packageFilename, LOAD_Verify | LOAD_NoWarn | LOAD_Quiet, nullptr);
    if (uasset)
    {
        return uasset;
    }

    // UAsset doesn't exist. Create one
    auto package = CreatePackage(nullptr, *packageName);
    if (package == nullptr)
    {
        UE_LOG(
            LogAcoustics,
            Error,
            TEXT("Failed to create package %s while importing %s, please manually create an AcousticData asset named "
                 "%s."),
            *packageName,
            *aceFilepath,
            *name);
        return nullptr;
    }

    auto asset = NewObject<UAcousticsData>(package, UAcousticsData::StaticClass(), *name, RF_Standalone | RF_Public);
    if (asset)
    {
        FAssetRegistryModule::AssetCreated(asset);
        asset->MarkPackageDirty();
        asset->PostEditChange();
        asset->AddToRoot();
        UPackage::SavePackage(package, nullptr, RF_Standalone, *packageFilename);
    }
    else
    {
        UE_LOG(
            LogAcoustics,
            Error,
            TEXT("Failed to import %s, please manually create an AcousticData asset named %s."),
            *aceFilepath,
            *name);
    }
    return asset;
}