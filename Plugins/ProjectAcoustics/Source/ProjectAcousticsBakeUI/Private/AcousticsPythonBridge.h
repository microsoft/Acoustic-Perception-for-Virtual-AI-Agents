// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once
#include "UObject/Object.h"
#include "AcousticsPythonBridge.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Azure Account"))
struct FAzureCredentials
{
    GENERATED_BODY()

    // clang-format off
    UPROPERTY(
        EditAnywhere, BlueprintReadWrite, NonTransactional, Transient,
        meta = (DisplayName = "Batch Account Name", tooltip = "Name of Azure Batch account from Azure Portal"),
        Category = "Azure Account")
    FString batch_name;

    UPROPERTY(
        EditAnywhere, BlueprintReadWrite, NonTransactional, Transient,
        meta = (DisplayName = "Batch Account URL", tooltip = "URL for the Azure Batch account from Azure Portal"),
        Category = "Azure Account")
    FString batch_url;

    UPROPERTY(
        EditAnywhere, BlueprintReadWrite, NonTransactional, Transient,
        meta = (DisplayName = "Batch Access Key", tooltip = "Batch account access key from Azure Portal"),
        Category = "Azure Account")
    FString batch_key;

    UPROPERTY(
        EditAnywhere, BlueprintReadWrite, NonTransactional, Transient,
        meta =
            (DisplayName = "Storage Account Name",
             tooltip = "Name of the Azure storage account associated with the Azure Batch account on Azure Portal"),
        Category = "Azure Account")
    FString storage_name;

    UPROPERTY(
        EditAnywhere, BlueprintReadWrite, NonTransactional, Transient,
        meta =
            (DisplayName = "Storage Access Key",
             tooltip = "Name of the Azure storage account associated with the Azure Batch account on Azure Portal"),
        Category = "Azure Account")
    FString storage_key;

    UPROPERTY(
        EditAnywhere, BlueprintReadWrite, NonTransactional, Transient,
        meta =
            (DisplayName = "Project Acoustics Toolset",
             tooltip = "Specific version of Microsoft Project Acoustics used for simulation processing"),
        Category = "Azure Account")
    FString toolset_version;

    UPROPERTY(
        EditAnywhere, BlueprintReadWrite, NonTransactional, Transient,
        meta =
            (DisplayName = "Custom Azure Container Registry Server",
             tooltip = "If using non-default Project Acoustics Toolset, specify the Azure Container Registry where it is hosted. Otherwise, leave blank"),
        Category = "Azure Account")
    FString acr_server;

    UPROPERTY(
        EditAnywhere, BlueprintReadWrite, NonTransactional, Transient,
        meta =
            (DisplayName = "Custom Azure Container Registry Account",
             tooltip = "If using non-default Project Acoustics Toolset, the account to use for authentication to the custom container registry. Otherwise, leave blank"),
        Category = "Azure Account")
    FString acr_account;

    UPROPERTY(
        EditAnywhere, BlueprintReadWrite, NonTransactional, Transient,
        meta =
            (DisplayName = "Custom Azure Container Registry Key",
             tooltip = "If using non-default Project Acoustics Toolset, the key to use for authentication to the custom container registry. Otherwise, leave blank"),
        Category = "Azure Account")
    FString acr_key;
    // clang-format on
};

USTRUCT(BlueprintType)
struct FProbeSampling
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    float horizontal_spacing_min;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    float horizontal_spacing_max;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    float vertical_spacing;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    float min_height_above_ground;
};

USTRUCT(BlueprintType)
struct FSimulationRegion
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FBox small;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FBox large;
};

USTRUCT(BlueprintType)
struct FSimulationParameters
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    float mesh_unit_adjustment;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    float scene_scale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    float speed_of_sound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    int frequency;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    float receiver_spacing;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    float voxel_map_resolution;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FProbeSampling probe_spacing;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FSimulationRegion simulation_region;
};

USTRUCT(BlueprintType)
struct FComputePoolConfiguration
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FString vm_size;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    int nodes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    bool use_low_priority_nodes;
};

USTRUCT(BlueprintType)
struct FJobConfiguration
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    int probe_count;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FString vox_file;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FString config_file;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FString prefix;
};

USTRUCT(BlueprintType)
struct FProjectConfiguration
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    TMap<FString, FString> level_prefix_map;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FString plugins_dir;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FString content_dir;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FString game_content_dir;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FString config_dir;
};

USTRUCT(BlueprintType)
struct FActiveJobInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FString job_id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FString submit_time;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FString prefix;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    bool submit_pending;
};

USTRUCT(BlueprintType)
struct FAzureCallStatus
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    bool succeeded;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FString message;
};

UCLASS(Blueprintable)
class UAcousticsPythonBridge : public UObject
{
    GENERATED_BODY()

    // Added edit mode for source control access.
private:
    class FAcousticsEdMode* m_AcousticsEditMode;

public:
    UFUNCTION(BlueprintCallable, Category = Python)
    static UAcousticsPythonBridge* Get();

    void Initialize();

    // These methods are projected up to Python.
    // Please use snake_case for all projected members.
    UFUNCTION(BlueprintImplementableEvent, Category = Python)
    void initialize_projection() const;

    UFUNCTION(BlueprintImplementableEvent, Category = Python)
    void load_configuration() const;

    UFUNCTION(BlueprintImplementableEvent, Category = Python)
    void save_configuration() const;

    UFUNCTION(BlueprintImplementableEvent, Category = Python)
    void update_azure_credentials() const;

    UFUNCTION(BlueprintImplementableEvent, Category = Python)
    float estimate_processing_time() const;

    UFUNCTION(BlueprintImplementableEvent, Category = Python)
    void submit_for_processing() const;

    UFUNCTION(BlueprintImplementableEvent, Category = Python)
    void cancel_job() const;

    UFUNCTION(BlueprintImplementableEvent, Category = Python)
    void update_job_status() const;

    UFUNCTION(BlueprintCallable, Category = "Acoustics")
    void create_ace_asset(FString acePath);

    UFUNCTION(BlueprintCallable, Category = "Acoustics")
    void show_readonly_ace_dialog();

    void SetProjectConfiguration(const FProjectConfiguration& config);
    const FProjectConfiguration& GetProjectConfiguration() const
    {
        return project_config;
    }

    void SetAzureCredentials(const FAzureCredentials& creds);
    const FAzureCredentials& GetAzureCredentials() const
    {
        return azure_credentials;
    }

    void SetSimulationParameters(const FSimulationParameters& config);
    const FSimulationParameters& GetSimulationParameters() const
    {
        return simulation_parameters;
    }

    void SetComputePoolConfiguration(const FComputePoolConfiguration& config);
    const FComputePoolConfiguration& GetComputePoolConfiguration() const
    {
        return compute_pool_configuration;
    }

    void SetJobConfiguration(const FJobConfiguration& config);
    const FJobConfiguration& GetJobConfiguration() const
    {
        return job_configuration;
    }

    const FActiveJobInfo& GetActiveJobInfo() const
    {
        return active_job_info;
    }

    const FAzureCallStatus& GetCurrentStatus()
    {
        // Update and return latest status
        update_job_status();
        return current_status;
    }

    // Unreal's Python reflection will crash if passing parameters to reflected functions.
    // Workaround is to setup public reflected properties instead.
    // Please use Accessors/Mutators instead of direct access, if/when UE fixes
    // this issue the only change required would be to make these properties private.
    // Python reflection also requires the member names to be in lower_snake_case format.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FProjectConfiguration project_config;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FAzureCredentials azure_credentials;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FSimulationParameters simulation_parameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FComputePoolConfiguration compute_pool_configuration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FJobConfiguration job_configuration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FActiveJobInfo active_job_info;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FAzureCallStatus current_status;
};