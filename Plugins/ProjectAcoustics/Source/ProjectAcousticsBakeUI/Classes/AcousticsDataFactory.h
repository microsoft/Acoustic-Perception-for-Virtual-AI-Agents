// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "Factories/Factory.h"
#include "AcousticsDataFactory.generated.h"

UCLASS(hidecategories = Object)
class UAcousticsDataFactory : public UFactory
{
    GENERATED_UCLASS_BODY()

#if CPP
    // UFactory Interface
    virtual UObject* FactoryCreateNew(
        UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context,
        FFeedbackContext* Warn) override;
#endif

    virtual bool FactoryCanImport(const FString& Filename) override;

    static UObject* ImportFromFile(const FString& filename);
};
