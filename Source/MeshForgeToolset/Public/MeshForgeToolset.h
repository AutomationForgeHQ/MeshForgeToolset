// MeshForge, for agents.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "ToolsetRegistry/ToolCallAsyncResult.h"
#include "MeshForgeToolsetTypes.h"
#include "MeshForgePipeline.h"
#include "MeshForgeToolset.generated.h"

/**
 * A generation batch, completed.
 *
 * A typed result rather than UToolCallAsyncResultString, so the agent gets the definitions and their
 * import outcomes as structure instead of prose it has to parse. The base class finds the schema by
 * reflecting over the property literally named `Value`.
 */
UCLASS(BlueprintType)
class MESHFORGETOOLSET_API UToolCallAsyncResultMeshGeneration : public UToolCallAsyncResult
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "MeshForge")
	bool SetValue(const FMeshGenerationReport& InValue)
	{
		return MaybeBroadcastSuccessfulCompletion(FMeshGenerationReport(InValue), Value);
	}

	UPROPERTY(BlueprintReadOnly, Category = "MeshForge")
	FMeshGenerationReport Value;
};

/** One picture, drawn and ingested. */
UCLASS(BlueprintType)
class MESHFORGETOOLSET_API UToolCallAsyncResultMeshImage : public UToolCallAsyncResult
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "MeshForge")
	bool SetValue(const FMeshImageInfo& InValue)
	{
		return MaybeBroadcastSuccessfulCompletion(FMeshImageInfo(InValue), Value);
	}

	UPROPERTY(BlueprintReadOnly, Category = "MeshForge")
	FMeshImageInfo Value;
};

/** One import, completed. */
UCLASS(BlueprintType)
class MESHFORGETOOLSET_API UToolCallAsyncResultMeshImport : public UToolCallAsyncResult
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "MeshForge")
	bool SetValue(const FMeshImportOutcome& InValue)
	{
		return MaybeBroadcastSuccessfulCompletion(FMeshImportOutcome(InValue), Value);
	}

	UPROPERTY(BlueprintReadOnly, Category = "MeshForge")
	FMeshImportOutcome Value;
};

/**
 * Generate static meshes from images and prompts, and finish them for a game.
 *
 * An adapter and nothing more: every tool here forwards to MeshForge's subsystem, which owns the
 * pipeline. Deliberately absent, and this is a design decision rather than an omission: there is no
 * tool that writes a credential, and none that deletes a generated asset. An agent that can spend
 * somebody's API budget should not also be able to store the key that pays for it, and a mesh that
 * took a minute of GPU time should be deleted by a person in the Content Browser.
 */
UCLASS(BlueprintType)
class MESHFORGETOOLSET_API UMeshForgeToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	virtual FString GetToolsetVersion() const override { return TEXT("0.0.1"); }

	// ---------------------------------------------------------------------------------------------
	// Discovery
	// ---------------------------------------------------------------------------------------------

	/**
	 * Every mesh provider installed, and whether each can generate right now.
	 *
	 * Call this before anything else. An empty list is normal rather than an error - MeshForge ships
	 * no provider of its own, so a project with no provider plugin enabled has none.
	 *
	 * @return One entry per provider. Read Caps.bRequiresImage before writing a prompt-only definition.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Discovery")
	static TArray<FMeshProviderInfo> ListMeshProviders();

	/**
	 * Every mesh definition in the project.
	 *
	 * @param PathFilter Content path to search under. Empty searches the project's MeshForge output folder.
	 * @return One entry per definition, with its status and what it has produced.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Discovery")
	static TArray<FMeshDefInfo> ListMeshDefinitions(const FString& PathFilter);

	/**
	 * One mesh definition in full.
	 *
	 * @param DefinitionPath Content path, as returned by List Mesh Definitions.
	 * @return Its authoring, its status and its last import.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Discovery")
	static FMeshDefInfo GetMeshDefinition(const FString& DefinitionPath);

	// ---------------------------------------------------------------------------------------------
	// Authoring
	// ---------------------------------------------------------------------------------------------

	/**
	 * Create a mesh definition. Does not generate anything.
	 *
	 * Creating and generating are separate on purpose: on a metered provider, generating is the call
	 * that costs money, and it should be a decision rather than a side effect.
	 *
	 * Describe the object rather than a picture of it. "A dented steel ammunition crate with rope
	 * handles" is a prop; "a photo of a crate on a white background, studio lighting" puts the studio
	 * into the mesh.
	 *
	 * @param AssetName Name for the new asset, e.g. "MD_AmmoCrate". Sanitised, and made unique.
	 * @param Spec What to generate and how to finish it. Leave fields at their defaults to inherit the project's.
	 * @return The definition just created.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Authoring")
	static FMeshDefInfo CreateMeshDefinition(const FString& AssetName, const FMeshDefSpec& Spec);

	/**
	 * Change how a definition is generated - seed, quality, polygon budget, texture size.
	 *
	 * Takes effect on the next generation; it does not change a mesh already imported.
	 *
	 * @param DefinitionPath Content path of the definition.
	 * @param Control The new settings, in full. Fields not set take their defaults.
	 * @return The definition as it now stands.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Authoring")
	static FMeshDefInfo SetMeshControl(const FString& DefinitionPath, const FMeshControl& Control);

	/**
	 * Change how a definition's mesh is finished - collision, lightmap UVs, Nanite, scale, pivot.
	 *
	 * Cheap, and worth reaching for before regenerating anything: none of this needs a GPU, costs
	 * money or waits on a provider. Call Refinish Mesh afterwards to apply it to a mesh already
	 * imported.
	 *
	 * @param DefinitionPath Content path of the definition.
	 * @param Finish The new settings, in full.
	 * @return The definition as it now stands.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Authoring")
	static FMeshDefInfo SetMeshFinishSettings(const FString& DefinitionPath, const FMeshFinishSettings& Finish);

	// ---------------------------------------------------------------------------------------------
	// Generation
	// ---------------------------------------------------------------------------------------------

	/**
	 * Estimate what generating these definitions would cost, without generating anything.
	 *
	 * Call this before Generate Meshes on any provider whose Caps say bIsMetered. On a local provider
	 * it reports zero, which is the honest answer rather than a refusal to say.
	 *
	 * @param DefinitionPaths Content paths of the definitions to price.
	 * @return One line per definition saying what it would spend and how long it would take.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Generation")
	static TArray<FString> EstimateMeshGeneration(const TArray<FString>& DefinitionPaths);

	/**
	 * Generate meshes for these definitions, then import the first success of each.
	 *
	 * **This is the call that spends.** On a metered provider every variant is billed the moment it
	 * is submitted, kept or discarded - call Estimate Mesh Generation first.
	 *
	 * Completes when every definition has finished or failed. A definition that fails does not stop
	 * the others; read each entry's LastError in the result.
	 *
	 * @param DefinitionPaths Content paths of the definitions to generate.
	 * @return The batch, its counts, and every definition's final state.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Generation")
	static UToolCallAsyncResultMeshGeneration* GenerateMeshes(const TArray<FString>& DefinitionPaths);

	// ---------------------------------------------------------------------------------------------
	// Import
	// ---------------------------------------------------------------------------------------------

	/**
	 * Import a take that has already been generated, replacing whatever this definition imported before.
	 *
	 * For choosing a different candidate than the one that was imported automatically.
	 *
	 * @param DefinitionPath Content path of the definition.
	 * @param MeshId Which take. Empty takes the selected one, or the only one that worked.
	 * @return What the import produced, including anything that succeeded imperfectly.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Import")
	static UToolCallAsyncResultMeshImport* ImportMeshCandidate(const FString& DefinitionPath, const FString& MeshId);

	/**
	 * Re-apply a definition's finish settings to the mesh it already imported.
	 *
	 * The cheap loop: collision, scale, pivot, Nanite and lightmap UVs all change here with no
	 * generation, no download and no money. Reach for this before regenerating.
	 *
	 * @param DefinitionPath Content path of the definition.
	 * @return What changed, and anything that could not be done.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Import")
	static FMeshImportOutcome RefinishMesh(const FString& DefinitionPath);

	/**
	 * Run a definition's post-processing chain, then re-import what comes out.
	 *
	 * **On a metered step this is the call that spends.** Retexturing is the common one, and it is
	 * the cheap way to iterate: the geometry is untouched, so three surfaces on one shape cost three
	 * retextures rather than three generations. Call List Pipelines with kind Post to see what is
	 * installed and what each costs, and Set Pipeline to put one on the Post stage.
	 *
	 * Works on whichever mesh the definition would use - the one supplied on its Mesh stage, or the
	 * chosen take if it generated one. A definition with neither is refused rather than started.
	 *
	 * Completes when the chain has finished and the result is imported, which on a hosted step is
	 * one to several minutes.
	 *
	 * @param DefinitionPath Content path of the definition. It needs at least one enabled Post step.
	 * @param StepIndex Zero-based index to run only that step with its configured input; -1 runs the chain.
	 * Missing saved prerequisites are refused; they are never run implicitly. Call List Post Inputs to choose another input.
	 * @return What the re-import produced, including anything that succeeded imperfectly.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Post")
	static UToolCallAsyncResultMeshImport* RunPostProcessing(const FString& DefinitionPath, int32 StepIndex = -1);

	/** List saved post outputs, including older takes predating per-step history. Select a static asset
	 * through ObjectTools on the step's InputMesh and set InputSource to SelectedMesh before running it. */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Post")
	static TArray<FMeshPostOutput> ListPostInputs(const FString& DefinitionPath);

	/**
	 * Set a definition's whole post-processing chain, in order, replacing whatever was there.
	 *
	 * **The whole list rather than one step, because the order is the meaning.** Unwrapping before
	 * retopology throws the UVs away; baking maps before decimating bakes detail the decimation then
	 * removes. An add-one-step tool would let a chain be built in an order nobody stated.
	 *
	 * Class names come from List Pipelines with kind Post. An empty list clears the stage, which is
	 * how a definition goes back to importing its mesh as generated.
	 *
	 * Creates the steps with their default settings; use the object property tools on the returned
	 * paths to configure them, then Run Post Processing.
	 *
	 * @param DefinitionPath     Content path of the definition.
	 * @param PipelineClassNames Class names in the order they should run. Empty clears the stage.
	 * @return The created step objects' paths, for the object property tools.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Post")
	static TArray<FString> SetPostPipelines(
		const FString& DefinitionPath, const TArray<FString>& PipelineClassNames);

	/**
	 * Import a glTF already on disk, finished the same way a generated one is.
	 *
	 * No provider and no generation - use this for a mesh somebody already has, from a web tool, a
	 * scan, or a colleague. It gets the same collision, lightmap UVs, Nanite decision, scale and
	 * pivot as anything MeshForge generates.
	 *
	 * @param AbsoluteFilePath A .glb or .gltf on disk. Anything else is refused.
	 * @param AssetName Name for the static mesh. Empty takes the file's own name.
	 * @param Finish How to finish it - collision, lightmap UVs, Nanite, scale, pivot.
	 * @return What the import produced, including anything that succeeded imperfectly.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Import")
	static FMeshImportOutcome ImportMeshFile(
		const FString& AbsoluteFilePath,
		const FString& AssetName,
		const FMeshFinishSettings& Finish);

	// ---------------------------------------------------------------------------------------------
	// Pipelines and pictures
	//
	// A pipeline is a C++ class rather than an asset to look up, so setting one instances it onto
	// the definition. These tools create and wire; they do not configure. Set Pipeline hands back
	// the object's path, and its settings are then written with the generic object property tools -
	// mirroring every vendor's fields into typed signatures here would rot the moment one of them
	// adds a setting, and the reflected options are already published in the result.
	// ---------------------------------------------------------------------------------------------

	/**
	 * Every pipeline class a stage could be set to, with what it costs and what it can be told.
	 *
	 * Call this before Set Pipeline. The two image pipelines differ by where the work happens and
	 * who pays - free on this machine's GPU against credits on somebody's account - and that is not
	 * recoverable from a class name.
	 *
	 * @param Kind Which stage. Image, Refine, Mesh or Post.
	 * @return The pipelines of that kind, in display-name order.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Pipelines")
	static TArray<FMeshPipelineInfo> ListPipelines(EMeshPipelineKind Kind);

	/**
	 * Put a pipeline on one of a definition's stages, replacing whatever was there.
	 *
	 * Marks the stages after it stale without discarding what they produced.
	 *
	 * @param DefinitionPath Content path of the definition.
	 * @param Stage Which stage to set. Only Concept and Mesh take a single pipeline.
	 * @param PipelineClassName A ClassName from List Pipelines. Empty clears the stage.
	 * @return The created object's path, for the object property tools. Empty when cleared.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Pipelines")
	static FString SetPipeline(const FString& DefinitionPath, EMeshStage Stage, const FString& PipelineClassName);

	/**
	 * Draw a reference image for a definition, through its concept pipeline.
	 *
	 * **This is the largest quality lever in the pipeline, and on a hosted pipeline it spends.** The
	 * same prompt through a free local model and through a hosted one produced a smooth blob and a
	 * crate with legible stencilled lettering. Read the pipeline's Cost before calling.
	 *
	 * Every picture becomes a texture asset and is added to the definition's gallery; the newest
	 * becomes the main image. Completes when the picture has been drawn and ingested, which is
	 * fifteen seconds to two minutes.
	 *
	 * @param DefinitionPath Content path of the definition. It needs a prompt and a concept pipeline.
	 * @return The picture that was drawn.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Pipelines")
	static UToolCallAsyncResultMeshImage* DrawConceptImage(const FString& DefinitionPath);

	/**
	 * Every picture a definition holds, and which of them its generator is shown.
	 *
	 * @param DefinitionPath Content path of the definition.
	 * @return The gallery, in the order the panel shows it.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Pipelines")
	static TArray<FMeshImageInfo> ListDefinitionImages(const FString& DefinitionPath);

	/**
	 * Choose which pictures the mesh generator is shown.
	 *
	 * **Extra views are worth being sceptical of.** They were tried three ways and each made the
	 * reconstruction measurably worse than the single picture it came from: a model fuses
	 * contradictory evidence rather than averaging it. Send them only when they are a genuine orbit
	 * of one object, and check MaxExtraViews in the result - a single-view generator drops them.
	 *
	 * @param DefinitionPath Content path of the definition.
	 * @param MainImagePath Content path of a texture in the definition's gallery. Empty leaves it.
	 * @param ExtraViewPaths Up to three more angles. An empty array clears them.
	 * @return What the generator will now be shown, or why the change was refused.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Pipelines")
	static FMeshImageSelection ChooseImages(
		const FString& DefinitionPath,
		const FString& MainImagePath,
		const TArray<FString>& ExtraViewPaths);

	/**
	 * What MeshForge is doing right now, and what it recently finished.
	 *
	 * Drawing and generating both return before the work is done, so this is how to tell a job that
	 * is still running from one that failed. A finished job stays listed for a while.
	 *
	 * @return Running jobs first, then the most recently finished.
	 */
	UFUNCTION(meta = (AICallable), Category = "MeshForge|Generation")
	static TArray<FMeshForgeJob> ListJobs();

};
