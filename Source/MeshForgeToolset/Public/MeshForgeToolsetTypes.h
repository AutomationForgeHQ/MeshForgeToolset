// What the tools hand back. Typed, because the signature is the schema an agent reads.

#pragma once

#include "CoreMinimal.h"
#include "MeshForgeTypes.h"
#include "MeshForgeToolsetTypes.generated.h"

/** One registered mesh provider, and whether it can be used right now. */
USTRUCT(BlueprintType)
struct FMeshProviderInfo
{
	GENERATED_BODY()

	/** Name a mesh definition. */
	UPROPERTY(BlueprintReadOnly, Category = "Provider")
	FName ProviderId;

	UPROPERTY(BlueprintReadOnly, Category = "Provider")
	FString DisplayName;

	/** False when generating would fail right now. Read Reason before submitting anything. */
	UPROPERTY(BlueprintReadOnly, Category = "Provider")
	bool bAvailable = false;

	/** Why it is not available, in a sentence a person can act on. Empty when it is. */
	UPROPERTY(BlueprintReadOnly, Category = "Provider")
	FString Reason;

	/**
	 * Options that belong to this provider alone, keyed for FMeshControl::Extra.
	 *
	 * The control struct only carries what every provider has, which leaves out most of what makes
	 * one worth choosing. These are the rest: read them before setting an extra, because a value
	 * outside a declared option's own set is not always refused - on at least one provider it
	 * silently changes the generation mode and doubles the price.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Provider")
	TArray<FMeshProviderOption> Options;

	/** What it can do. Check bRequiresImage before writing a prompt-only definition for it. */
	UPROPERTY(BlueprintReadOnly, Category = "Provider")
	FMeshProviderCaps Caps;

	/**
	 * True when this provider can draw its own reference image from a prompt.
	 *
	 * The field that decides whether a prompt-only definition works on an image-only provider.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Provider")
	bool bCanDrawConceptImages = false;
};

/** One mesh definition, enough to decide what to do with it without loading the asset. */
USTRUCT(BlueprintType)
struct FMeshDefInfo
{
	GENERATED_BODY()

	/** Content path. Pass this to any tool that takes a definition. */
	UPROPERTY(BlueprintReadOnly, Category = "Definition")
	FString Path;

	UPROPERTY(BlueprintReadOnly, Category = "Definition")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Definition")
	FString Prompt;

	/** The reference image, as a content path or a file path. Empty when there is none. */
	UPROPERTY(BlueprintReadOnly, Category = "Definition")
	FString SourceImage;

	UPROPERTY(BlueprintReadOnly, Category = "Definition")
	EMeshDefStatus Status = EMeshDefStatus::Draft;

	UPROPERTY(BlueprintReadOnly, Category = "Definition")
	FName ProviderId;

	/** Takes generated so far, and how many of them finished. */
	UPROPERTY(BlueprintReadOnly, Category = "Definition")
	int32 CandidateCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Definition")
	int32 UsableCandidateCount = 0;

	/** The imported static mesh, or empty when nothing has been imported yet. */
	UPROPERTY(BlueprintReadOnly, Category = "Definition")
	FString ImportedMesh;

	/** Why the last operation failed. Empty when the last one succeeded. */
	UPROPERTY(BlueprintReadOnly, Category = "Definition")
	FString LastError;

	/** What the last import produced, including anything that succeeded imperfectly. */
	UPROPERTY(BlueprintReadOnly, Category = "Definition")
	FMeshImportOutcome LastImport;
};

/** The outcome of asking for a batch of generations. */
USTRUCT(BlueprintType)
struct FMeshGenerationReport
{
	GENERATED_BODY()

	/**
	 * Empty when nothing was submitted.
	 *
	 * Check this first. The counts below are meaningless without it, and an empty batch id is the
	 * only signal that a submission that looked accepted in fact started nothing.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Generation")
	FString BatchId;

	UPROPERTY(BlueprintReadOnly, Category = "Generation")
	int32 Submitted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Generation")
	int32 Failed = 0;

	/** Definitions that finished, with their status and imported mesh. */
	UPROPERTY(BlueprintReadOnly, Category = "Generation")
	TArray<FMeshDefInfo> Definitions;

	UPROPERTY(BlueprintReadOnly, Category = "Generation")
	FString LastError;
};

/** One picture in a definition's gallery, and what job it currently has. */
USTRUCT(BlueprintType)
struct FMeshImageInfo
{
	GENERATED_BODY()

	/** Content path of the texture. Pass this to Choose Images. */
	UPROPERTY(BlueprintReadOnly, Category = "Image")
	FString Path;

	/** "Drawn" by the concept stage, "Added" by hand, or "Refined" from another picture. */
	UPROPERTY(BlueprintReadOnly, Category = "Image")
	FString Origin;

	/** True when this is the picture the mesh generator is shown. Exactly one is, or none yet. */
	UPROPERTY(BlueprintReadOnly, Category = "Image")
	bool bIsMain = false;

	/** Its position among the extra views, or -1 when it is not one. */
	UPROPERTY(BlueprintReadOnly, Category = "Image")
	int32 ViewIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Image")
	int32 Width = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Image")
	int32 Height = 0;
};

/**
 * One pipeline class a stage could be set to.
 *
 * A pipeline is a C++ class, not an asset to look up: choosing one instances it onto the definition
 * as a subobject. Set the stage with Set Pipeline, then configure the returned object through the
 * generic object property tools - the same create-and-wire split the rest of this family keeps,
 * because mirroring every vendor's settings into typed tool signatures would rot the moment one of
 * them adds a field.
 */
USTRUCT(BlueprintType)
struct FMeshPipelineInfo
{
	GENERATED_BODY()

	/** Pass this to Set Pipeline. The class name, e.g. "LocalImagePipeline". */
	UPROPERTY(BlueprintReadOnly, Category = "Pipeline")
	FString ClassName;

	UPROPERTY(BlueprintReadOnly, Category = "Pipeline")
	FString DisplayName;

	/** Which service carries it out. */
	UPROPERTY(BlueprintReadOnly, Category = "Pipeline")
	FName ProviderId;

	/** "Image", "Refine", "Mesh" or "Post". */
	UPROPERTY(BlueprintReadOnly, Category = "Pipeline")
	FString Kind;

	/**
	 * What one run costs, as a sentence. Empty where it is free.
	 *
	 * Read this before setting a pipeline on somebody's definition. The difference between the two
	 * image pipelines is free-on-this-machine against credits-on-an-account, and it is not
	 * recoverable from the class name.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Pipeline")
	FString Cost;

	/** Its settings, as the flat option list. Set them with the object property tools. */
	UPROPERTY(BlueprintReadOnly, Category = "Pipeline")
	TArray<FMeshProviderOption> Options;
};

/** Which pictures a definition will show its generator, after a change. */
USTRUCT(BlueprintType)
struct FMeshImageSelection
{
	GENERATED_BODY()

	/** The main picture's content path, or empty when none is chosen. */
	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	FString MainImage;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	TArray<FString> ExtraViews;

	/**
	 * How many extra views the definition's current generator can actually use.
	 *
	 * Zero on a single-view model, and everything in ExtraViews is then dropped at submit time with
	 * a line in the log. Check this before adding any: three pictures silently ignored looks exactly
	 * like three pictures used badly.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	int32 MaxExtraViews = 0;

	/** Why the change was refused, or empty when it was applied. */
	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	FString Error;
};
