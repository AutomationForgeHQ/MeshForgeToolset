#include "MeshForgeToolset.h"

#include "MeshDef.h"
#include "MeshPostPipeline.h"
#include "MeshForge.h"
#include "MeshForgeSettings.h"
#include "MeshForgeSubsystem.h"
#include "IMeshProvider.h"
#include "MeshForgePipeline.h"
#include "MeshImagePipeline.h"
#include "Engine/Texture2D.h"
#include "UObject/UObjectIterator.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Containers/Ticker.h"

namespace MeshForgeToolsetPrivate
{
	/**
	 * The subsystem, or a script error naming what to do instead.
	 *
	 * Errors are raised rather than returned throughout this file. An {"ok": false} envelope would
	 * pollute every success type and make the published schema lie about what a tool returns.
	 */
	static UMeshForgeSubsystem* RequireSubsystem()
	{
		UMeshForgeSubsystem* Subsystem = UMeshForgeSubsystem::Get();

		if (Subsystem == nullptr)
		{
			UKismetSystemLibrary::RaiseScriptError(
				TEXT("MeshForge is not available. This tool only works in the editor."));
		}

		return Subsystem;
	}

	/** Load a definition by content path, or raise an error that says how to find a valid one. */
	static UMeshDef* RequireDef(const FString& Path)
	{
		if (Path.IsEmpty())
		{
			UKismetSystemLibrary::RaiseScriptError(
				TEXT("No definition path was given. Call List Mesh Definitions for the valid paths."));
			return nullptr;
		}

		UMeshDef* Def = LoadObject<UMeshDef>(nullptr, *Path);

		if (Def == nullptr)
		{
			UKismetSystemLibrary::RaiseScriptError(FString::Printf(
				TEXT("No mesh definition at '%s'. Call List Mesh Definitions for the valid paths."),
				*Path));
		}

		return Def;
	}

	static FMeshDefInfo Describe(const UMeshDef* Def)
	{
		FMeshDefInfo Info;

		if (Def == nullptr)
		{
			return Info;
		}

		Info.Path = Def->GetPathName();
		Info.Name = Def->GetName();
		Info.Prompt = Def->Prompt;
		Info.Status = Def->Status;
		Info.ProviderId = Def->ProviderId;
		Info.CandidateCount = Def->Candidates.Num();
		Info.UsableCandidateCount = Def->CountUsableCandidates();
		Info.LastError = Def->LastError;
		Info.LastImport = Def->LastImport;

		Info.SourceImage = Def->SourceImage.IsNull()
			? Def->SourceImagePath
			: Def->SourceImage.ToString();

		Info.ImportedMesh = Def->ImportedMesh.IsNull() ? FString() : Def->ImportedMesh.ToString();

		return Info;
	}

}

TArray<FMeshProviderInfo> UMeshForgeToolset::ListMeshProviders()
{
	TArray<FMeshProviderInfo> Result;

	FMeshForgeModule* Module = FMeshForgeModule::GetPtr();

	if (Module == nullptr)
	{
		return Result;
	}

	for (const FName Id : Module->GetProviderIds())
	{
		TSharedPtr<IMeshProvider> Provider = Module->FindProvider(Id);

		if (!Provider.IsValid())
		{
			continue;
		}

		FMeshProviderInfo Info;
		Info.ProviderId = Id;
		Info.DisplayName = Provider->GetDisplayName();
		Info.bAvailable = Provider->IsAvailable(Info.Reason);
		Info.Caps = Provider->GetCaps();
		Info.Options = Provider->GetOptions();
		Info.bCanDrawConceptImages = Provider->SupportsConceptImages();

		Result.Add(MoveTemp(Info));
	}

	return Result;
}

TArray<FMeshDefInfo> UMeshForgeToolset::ListMeshDefinitions(const FString& PathFilter)
{
	TArray<FMeshDefInfo> Result;

	const FAssetRegistryModule& Registry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	FARFilter Filter;
	Filter.ClassPaths.Add(UMeshDef::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;

	// An empty filter means the project's own output folder rather than the whole of /Game, because
	// scanning every asset in a large project to find four definitions is slow and the answer is the
	// same.
	Filter.PackagePaths.Add(FName(*(PathFilter.IsEmpty()
		? UMeshForgeSettings::Get()->GetDefinitionsPath()
		: PathFilter)));

	TArray<FAssetData> Assets;
	Registry.Get().GetAssets(Filter, Assets);

	for (const FAssetData& Asset : Assets)
	{
		if (const UMeshDef* Def = Cast<UMeshDef>(Asset.GetAsset()))
		{
			Result.Add(MeshForgeToolsetPrivate::Describe(Def));
		}
	}

	return Result;
}

FMeshDefInfo UMeshForgeToolset::GetMeshDefinition(const FString& DefinitionPath)
{
	const UMeshDef* Def = MeshForgeToolsetPrivate::RequireDef(DefinitionPath);
	return MeshForgeToolsetPrivate::Describe(Def);
}

FMeshDefInfo UMeshForgeToolset::CreateMeshDefinition(const FString& AssetName, const FMeshDefSpec& Spec)
{
	FMeshDefInfo Info;

	UMeshForgeSubsystem* Subsystem = MeshForgeToolsetPrivate::RequireSubsystem();
	if (Subsystem == nullptr)
	{
		return Info;
	}

	if (Spec.Prompt.IsEmpty() && Spec.SourceImagePath.IsEmpty())
	{
		UKismetSystemLibrary::RaiseScriptError(
			TEXT("A mesh definition needs a prompt, a source image, or both. Call List Mesh Providers "
				 "to see whether the provider you intend to use requires an image."));
		return Info;
	}

	FString Error;
	UMeshDef* Def = Subsystem->CreateMeshDef(AssetName, Spec, Error);

	if (Def == nullptr)
	{
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("Could not create the definition: %s"), *Error));
		return Info;
	}

	return MeshForgeToolsetPrivate::Describe(Def);
}

FMeshDefInfo UMeshForgeToolset::SetMeshControl(const FString& DefinitionPath, const FMeshControl& Control)
{
	UMeshDef* Def = MeshForgeToolsetPrivate::RequireDef(DefinitionPath);

	if (Def == nullptr)
	{
		return FMeshDefInfo();
	}

	Def->Control = Control;
	Def->MarkPackageDirty();

	return MeshForgeToolsetPrivate::Describe(Def);
}

FMeshDefInfo UMeshForgeToolset::SetMeshFinishSettings(
	const FString& DefinitionPath, const FMeshFinishSettings& Finish)
{
	UMeshDef* Def = MeshForgeToolsetPrivate::RequireDef(DefinitionPath);

	if (Def == nullptr)
	{
		return FMeshDefInfo();
	}

	Def->Finish = Finish;
	Def->MarkPackageDirty();

	return MeshForgeToolsetPrivate::Describe(Def);
}

TArray<FString> UMeshForgeToolset::EstimateMeshGeneration(const TArray<FString>& DefinitionPaths)
{
	TArray<FString> Lines;

	UMeshForgeSubsystem* Subsystem = MeshForgeToolsetPrivate::RequireSubsystem();
	if (Subsystem == nullptr)
	{
		return Lines;
	}

	for (const FString& Path : DefinitionPaths)
	{
		UMeshDef* Def = LoadObject<UMeshDef>(nullptr, *Path);

		if (Def == nullptr)
		{
			Lines.Add(FString::Printf(TEXT("%s: no such definition."), *Path));
			continue;
		}

		TSharedPtr<IMeshProvider> Provider = Subsystem->ResolveProvider(Def);

		if (!Provider.IsValid())
		{
			Lines.Add(FString::Printf(
				TEXT("%s: no provider. Call List Mesh Providers, then Set Mesh Control or set one in "
					 "Project Settings."),
				*Def->GetName()));
			continue;
		}

		const FMeshProviderCaps Caps = Provider->GetCaps();
		const int32 Takes = Caps.bSupportsVariants ? FMath::Max(1, Def->Variants) : 1;

		// A provider that publishes a price list can quote one; most cannot, and seconds are then
		// the only unit this can be honest about.
		// The control the definition would actually be generated with, not its raw advanced settings:
		// pricing the second quotes a request nobody is going to send. And the chosen picture, from
		// wherever it came - a definition whose image is in the gallery was reported as having none.
		FString Model;
		const FMeshControl Control = Subsystem->ResolveControl(Def, Model);

		const bool bHasImage = Subsystem->HasReferenceImage(Def);
		const FString PerTake =
			Provider->DescribeCost(Control, bHasImage, Subsystem->UsableExtraViews(Def));

		FString Cost;

		if (!Caps.bIsMetered)
		{
			Cost = FString::Printf(TEXT("free - %s runs locally"), *Provider->GetDisplayName());
		}
		else if (!PerTake.IsEmpty())
		{
			Cost = FString::Printf(TEXT("%d billable generation%s on %s, %s"),
				Takes, Takes == 1 ? TEXT("") : TEXT("s"), *Provider->GetDisplayName(), *PerTake);
		}
		else
		{
			Cost = FString::Printf(TEXT("%d billable generation%s on %s"),
				Takes, Takes == 1 ? TEXT("") : TEXT("s"), *Provider->GetDisplayName());
		}

		Lines.Add(FString::Printf(TEXT("%s: %s, roughly %ds."),
			*Def->GetName(), *Cost, Takes * FMath::Max(1, Caps.TypicalSecondsPerMesh)));
	}

	return Lines;
}

UToolCallAsyncResultMeshGeneration* UMeshForgeToolset::GenerateMeshes(const TArray<FString>& DefinitionPaths)
{
	UToolCallAsyncResultMeshGeneration* Result = NewObject<UToolCallAsyncResultMeshGeneration>();

	UMeshForgeSubsystem* Subsystem = UMeshForgeSubsystem::Get();

	if (Subsystem == nullptr)
	{
		Result->SetError(TEXT("MeshForge is not available. This tool only works in the editor."));
		return Result;
	}

	TArray<UMeshDef*> Defs;
	TArray<FString> Missing;

	for (const FString& Path : DefinitionPaths)
	{
		if (UMeshDef* Def = LoadObject<UMeshDef>(nullptr, *Path))
		{
			Defs.Add(Def);
		}
		else
		{
			Missing.Add(Path);
		}
	}

	if (Defs.Num() == 0)
	{
		Result->SetError(FString::Printf(
			TEXT("None of those paths is a mesh definition (%s). Call List Mesh Definitions for the "
				 "valid paths."),
			*FString::Join(Missing, TEXT(", "))));
		return Result;
	}

	const FMeshBatchSubmission Submission = Subsystem->GenerateMeshes(Defs);

	if (Submission.BatchId.IsEmpty())
	{
		// The one failure worth being loud about: nothing was submitted, so nothing will ever
		// complete, and a tool that waited here would hang until its deadline.
		Result->SetError(FString::Printf(
			TEXT("Nothing was submitted. %s Call List Mesh Providers to see what is available and why."),
			*Submission.LastError));
		return Result;
	}

	// Held against the GC while the ticker owns it - the collector cannot see a raw pointer captured
	// in a lambda. Released on every exit path below.
	Result->AddToRoot();

	const FString BatchId = Submission.BatchId;
	const double Deadline = FPlatformTime::Seconds()
		+ static_cast<double>(FMath::Max(60, UMeshForgeSettings::Get()->JobTimeoutSeconds)) + 120.0;

	TArray<TWeakObjectPtr<UMeshDef>> Watched;
	for (UMeshDef* Def : Defs)
	{
		Watched.Add(Def);
	}

	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[Result, BatchId, Watched, Submission, Deadline](float) -> bool
	{
		UMeshForgeSubsystem* Live = UMeshForgeSubsystem::Get();
		const bool bRunning = Live != nullptr && Live->IsBatchRunning(BatchId);
		const bool bExpired = FPlatformTime::Seconds() > Deadline;

		if (bRunning && !bExpired)
		{
			return true;
		}

		FMeshGenerationReport Report;
		Report.BatchId = BatchId;
		Report.Submitted = Submission.Submitted;
		Report.Failed = Submission.Failed;
		Report.LastError = Submission.LastError;

		for (const TWeakObjectPtr<UMeshDef>& Weak : Watched)
		{
			if (const UMeshDef* Def = Weak.Get())
			{
				Report.Definitions.Add(MeshForgeToolsetPrivate::Describe(Def));
			}
		}

		if (bExpired)
		{
			Result->SetError(FString::Printf(
				TEXT("Gave up waiting on batch %s. The jobs may still be running; call Get Mesh "
					 "Definition on each path to see where they got to."),
				*BatchId));
		}
		else
		{
			Result->SetValue(Report);
		}

		Result->RemoveFromRoot();
		return false;
	}),
		1.0f);

	return Result;
}

UToolCallAsyncResultMeshImport* UMeshForgeToolset::ImportMeshCandidate(
	const FString& DefinitionPath, const FString& MeshId)
{
	UToolCallAsyncResultMeshImport* Result = NewObject<UToolCallAsyncResultMeshImport>();

	UMeshForgeSubsystem* Subsystem = UMeshForgeSubsystem::Get();

	if (Subsystem == nullptr)
	{
		Result->SetError(TEXT("MeshForge is not available. This tool only works in the editor."));
		return Result;
	}

	UMeshDef* Def = LoadObject<UMeshDef>(nullptr, *DefinitionPath);

	if (Def == nullptr)
	{
		Result->SetError(FString::Printf(
			TEXT("No mesh definition at '%s'. Call List Mesh Definitions for the valid paths."),
			*DefinitionPath));
		return Result;
	}

	Result->AddToRoot();

	Subsystem->ImportCandidate(Def, MeshId, [Result](const FMeshImportOutcome& Outcome)
	{
		if (Outcome.bSuccess)
		{
			Result->SetValue(Outcome);
		}
		else
		{
			Result->SetError(Outcome.Error);
		}

		Result->RemoveFromRoot();
	});

	return Result;
}

TArray<FMeshPostOutput> UMeshForgeToolset::ListPostInputs(const FString& DefinitionPath)
{
	UMeshForgeSubsystem* S = MeshForgeToolsetPrivate::RequireSubsystem();
	UMeshDef* Def = MeshForgeToolsetPrivate::RequireDef(DefinitionPath);
	return S && Def ? S->GetPostInputChoices(Def) : TArray<FMeshPostOutput>();
}

UToolCallAsyncResultMeshImport* UMeshForgeToolset::RunPostProcessing(const FString& DefinitionPath, int32 StepIndex)
{
	UToolCallAsyncResultMeshImport* Result = NewObject<UToolCallAsyncResultMeshImport>();

	UMeshForgeSubsystem* Subsystem = MeshForgeToolsetPrivate::RequireSubsystem();
	UMeshDef* Def = MeshForgeToolsetPrivate::RequireDef(DefinitionPath);

	if (Subsystem == nullptr || Def == nullptr)
	{
		Result->SetError(TEXT("No definition to post-process."));
		return Result;
	}

	FString Error;
	const FGuid JobId = StepIndex == -1 ? Subsystem->StartPostProcessing(Def, Error) : Subsystem->RunPostStep(Def, StepIndex, Error);

	if (!JobId.IsValid())
	{
		Result->SetError(Error);
		return Result;
	}

	// The garbage collector cannot see a raw pointer held only by a lambda, and this one has to
	// survive until a job finishes minutes from now. Cleared on the path that completes it.
	Result->AddToRoot();

	TWeakObjectPtr<UMeshDef> WeakDef(Def);
	const double Deadline = FPlatformTime::Seconds() + 3600.0;
	// A ticker owns the callback until it returns. Removing a multicast callback from inside
	// itself can destroy its captures while completion is still accessing them.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Result, JobId, WeakDef, Deadline](float) -> bool
	{
		UMeshForgeSubsystem* Live = UMeshForgeSubsystem::Get();
		if (Live == nullptr || !WeakDef.IsValid() || FPlatformTime::Seconds() > Deadline)
		{
			Result->SetError(TEXT("Post-processing wait ended. Call List Jobs to inspect its status."));
			Result->RemoveFromRoot();
			return false;
		}

		const TArray<FMeshForgeJob> Snapshot = Live->GetJobs();

		const FMeshForgeJob* Job = Snapshot.FindByPredicate(
			[&JobId](const FMeshForgeJob& Candidate) { return Candidate.Id == JobId; });

		if (Job == nullptr || Job->State == EMeshForgeJobState::Running)
		{
			return true;
		}

		UMeshDef* Target = WeakDef.Get();

		if (Job->State == EMeshForgeJobState::Succeeded && Target != nullptr)
		{
			// The definition's own record of what the re-import produced. The post stage writes it
			// before the job is marked finished, so by here it is the chain's result and not a
			// previous one.
			Result->SetValue(Target->LastImport);
		}
		else
		{
			Result->SetError(Job->Error.IsEmpty()
				? TEXT("Post-processing failed and gave no reason.")
				: Job->Error);
		}

		Result->RemoveFromRoot();
		return false;
	}), 0.1f);

	return Result;
}

FMeshImportOutcome UMeshForgeToolset::RefinishMesh(const FString& DefinitionPath)
{
	FMeshImportOutcome Outcome;

	UMeshForgeSubsystem* Subsystem = MeshForgeToolsetPrivate::RequireSubsystem();
	UMeshDef* Def = MeshForgeToolsetPrivate::RequireDef(DefinitionPath);

	if (Subsystem == nullptr || Def == nullptr)
	{
		return Outcome;
	}

	Outcome = Subsystem->RefinishMesh(Def);

	if (!Outcome.bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(
			TEXT("%s If nothing has been imported yet, call Generate Meshes or Import Mesh Candidate first."),
			*Outcome.Error));
	}

	return Outcome;
}

FMeshImportOutcome UMeshForgeToolset::ImportMeshFile(
	const FString& AbsoluteFilePath,
	const FString& AssetName,
	const FMeshFinishSettings& Finish)
{
	FMeshImportOutcome Outcome;

	UMeshForgeSubsystem* Subsystem = MeshForgeToolsetPrivate::RequireSubsystem();

	if (Subsystem == nullptr)
	{
		return Outcome;
	}

	Outcome = Subsystem->ImportMeshFile(AbsoluteFilePath, AssetName, Finish);

	if (!Outcome.bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Outcome.Error);
	}

	return Outcome;
}

// -------------------------------------------------------------------------------------------------
// Pipelines and pictures
// -------------------------------------------------------------------------------------------------

namespace MeshForgePipelineToolsPrivate
{
	static FString KindName(EMeshPipelineKind Kind)
	{
		return StaticEnum<EMeshPipelineKind>()->GetNameStringByValue(static_cast<int64>(Kind));
	}

	/** Every concrete pipeline class of one kind, in display-name order. */
	static TArray<UClass*> ClassesOfKind(EMeshPipelineKind Kind)
	{
		TArray<UClass*> Found;

		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;

			if (!Class->IsChildOf(UMeshForgePipeline::StaticClass())
				|| Class == UMeshForgePipeline::StaticClass()
				|| Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				continue;
			}

			if (const UMeshForgePipeline* Default = Class->GetDefaultObject<UMeshForgePipeline>())
			{
				if (Default->GetKind() == Kind)
				{
					Found.Add(Class);
				}
			}
		}

		Found.Sort([](const UClass& A, const UClass& B)
		{
			return A.GetDisplayNameText().CompareTo(B.GetDisplayNameText()) < 0;
		});

		return Found;
	}

	static FMeshImageInfo Describe(const UMeshDef* Def, const TSoftObjectPtr<UTexture2D>& Image)
	{
		FMeshImageInfo Info;
		Info.Path = Image.ToString();

		if (Def->ConceptImages.Contains(Image))
		{
			Info.Origin = TEXT("Drawn");
		}
		else if (Def->ReferenceImages.Contains(Image))
		{
			Info.Origin = TEXT("Refined");
		}
		else
		{
			Info.Origin = TEXT("Added");
		}

		Info.bIsMain   = (Image == Def->ResolveMainImage());
		Info.ViewIndex = Def->ExtraViews.IndexOfByKey(Image);

		if (UTexture2D* Texture = Image.LoadSynchronous())
		{
			// The *source* size, not GetSizeX. That reports whatever mip streaming has decided to
			// keep resident, which for a picture nobody has looked at yet is 32 pixels - and an
			// agent reading that would conclude the reference was useless.
#if WITH_EDITORONLY_DATA
			if (Texture->Source.IsValid())
			{
				Info.Width  = Texture->Source.GetSizeX();
				Info.Height = Texture->Source.GetSizeY();
			}
			else
#endif
			{
				Info.Width  = Texture->GetSizeX();
				Info.Height = Texture->GetSizeY();
			}
		}

		return Info;
	}
}

TArray<FMeshPipelineInfo> UMeshForgeToolset::ListPipelines(EMeshPipelineKind Kind)
{
	TArray<FMeshPipelineInfo> Out;

	for (UClass* Class : MeshForgePipelineToolsPrivate::ClassesOfKind(Kind))
	{
		const UMeshForgePipeline* Default = Class->GetDefaultObject<UMeshForgePipeline>();

		FMeshPipelineInfo Info;
		Info.ClassName   = Class->GetName();
		Info.DisplayName = Class->GetDisplayNameText().ToString();
		Info.ProviderId  = Default->GetProviderId();
		Info.Kind        = MeshForgePipelineToolsPrivate::KindName(Kind);
		Info.Cost        = Default->DescribeCost();
		Info.Options     = Default->DescribeOptions();

		Out.Add(Info);
	}

	return Out;
}

FString UMeshForgeToolset::SetPipeline(
	const FString& DefinitionPath, EMeshStage Stage, const FString& PipelineClassName)
{
	UMeshDef* Def = MeshForgeToolsetPrivate::RequireDef(DefinitionPath);
	if (Def == nullptr)
	{
		return FString();
	}

	if (Stage != EMeshStage::Concept && Stage != EMeshStage::Mesh)
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(
			TEXT("The %s stage takes an ordered list of pipelines rather than one, and the order "
				 "changes what it produces - so it is edited on the asset rather than through this "
				 "tool. Concept and Mesh each take exactly one."),
			*StaticEnum<EMeshStage>()->GetNameStringByValue(static_cast<int64>(Stage))));
		return FString();
	}

	const EMeshPipelineKind Wanted =
		(Stage == EMeshStage::Concept) ? EMeshPipelineKind::Image : EMeshPipelineKind::Mesh;

	UMeshForgePipeline* Instance = nullptr;

	if (!PipelineClassName.IsEmpty())
	{
		const TArray<UClass*> Candidates = MeshForgePipelineToolsPrivate::ClassesOfKind(Wanted);

		UClass* const* Match = Candidates.FindByPredicate(
			[&PipelineClassName](const UClass* Class) { return Class->GetName() == PipelineClassName; });

		if (Match == nullptr)
		{
			UKismetSystemLibrary::RaiseScriptError(FString::Printf(
				TEXT("No %s pipeline called %s. Call List Pipelines for the valid class names."),
				*MeshForgePipelineToolsPrivate::KindName(Wanted), *PipelineClassName));
			return FString();
		}

		// Outered to the definition, which is what makes it a subobject saved inside that asset.
		Instance = NewObject<UMeshForgePipeline>(Def, *Match, NAME_None, RF_Transactional);
	}

	Def->Modify();

	if (Stage == EMeshStage::Concept)
	{
		Def->ConceptPipeline = Cast<UMeshImagePipeline>(Instance);
	}
	else
	{
		Def->MeshPipeline = Instance;
	}

	// Changing what a stage would do changes what everything after it was made from.
	Def->RefreshStaleness();
	Def->MarkPackageDirty();

	return Instance ? Instance->GetPathName() : FString();
}

TArray<FString> UMeshForgeToolset::SetPostPipelines(
	const FString& DefinitionPath, const TArray<FString>& PipelineClassNames)
{
	TArray<FString> Created;

	UMeshDef* Def = MeshForgeToolsetPrivate::RequireDef(DefinitionPath);

	if (Def == nullptr)
	{
		return Created;
	}

	const TArray<UClass*> Candidates =
		MeshForgePipelineToolsPrivate::ClassesOfKind(EMeshPipelineKind::Post);

	// Resolved in full before anything is written, so a typo in the third name does not leave a
	// definition holding the first two and a half-built chain nobody asked for.
	TArray<UClass*> Resolved;

	for (const FString& Name : PipelineClassNames)
	{
		UClass* const* Match = Candidates.FindByPredicate(
			[&Name](const UClass* Class) { return Class->GetName() == Name; });

		if (Match == nullptr)
		{
			UKismetSystemLibrary::RaiseScriptError(FString::Printf(
				TEXT("No post-processing pipeline called %s. Call List Pipelines with kind Post for "
					 "the valid class names."), *Name));
			return Created;
		}

		Resolved.Add(*Match);
	}

	Def->Modify();
	Def->PostPipelines.Reset();

	for (UClass* Class : Resolved)
	{
		// Outered to the definition, which is what makes each step a subobject saved inside that
		// asset rather than a shared one.
		if (UMeshPostPipeline* Step =
				NewObject<UMeshPostPipeline>(Def, Class, NAME_None, RF_Transactional))
		{
			Def->PostPipelines.Add(Step);
			Created.Add(Step->GetPathName());
		}
	}

	// Changing what a stage would do changes what everything after it was made from.
	Def->RefreshStaleness();
	Def->MarkPackageDirty();

	return Created;
}

UToolCallAsyncResultMeshImage* UMeshForgeToolset::DrawConceptImage(const FString& DefinitionPath)
{
	UToolCallAsyncResultMeshImage* Result = NewObject<UToolCallAsyncResultMeshImage>();

	UMeshForgeSubsystem* Subsystem = MeshForgeToolsetPrivate::RequireSubsystem();
	UMeshDef* Def = MeshForgeToolsetPrivate::RequireDef(DefinitionPath);

	if (Subsystem == nullptr || Def == nullptr)
	{
		Result->SetError(TEXT("No definition to draw for."));
		return Result;
	}

	FString Error;
	const FGuid JobId = Subsystem->StartConceptDraw(Def, Error);

	if (!JobId.IsValid())
	{
		Result->SetError(Error);
		return Result;
	}

	// The garbage collector cannot see a raw pointer held only by a lambda, and this one has to
	// survive until a job finishes minutes from now. Cleared on the path that completes it.
	Result->AddToRoot();

	TWeakObjectPtr<UMeshDef> WeakDef(Def);
	TSharedRef<FDelegateHandle> Handle = MakeShared<FDelegateHandle>();

	*Handle = Subsystem->OnJobsChanged.AddLambda([Result, JobId, WeakDef, Handle]()
	{
		UMeshForgeSubsystem* Live = UMeshForgeSubsystem::Get();
		if (Live == nullptr)
		{
			return;
		}

		const TArray<FMeshForgeJob> Snapshot = Live->GetJobs();

		const FMeshForgeJob* Job = Snapshot.FindByPredicate(
			[&JobId](const FMeshForgeJob& Candidate) { return Candidate.Id == JobId; });

		if (Job == nullptr || Job->State == EMeshForgeJobState::Running)
		{
			return;
		}

		Live->OnJobsChanged.Remove(*Handle);

		UMeshDef* Target = WeakDef.Get();

		if (Job->State == EMeshForgeJobState::Succeeded && Target != nullptr)
		{
			Result->SetValue(
				MeshForgePipelineToolsPrivate::Describe(Target, Target->ResolveMainImage()));
		}
		else
		{
			Result->SetError(Job->Error.IsEmpty()
				? TEXT("The picture could not be drawn.")
				: Job->Error);
		}

		Result->RemoveFromRoot();
	});

	return Result;
}

TArray<FMeshImageInfo> UMeshForgeToolset::ListDefinitionImages(const FString& DefinitionPath)
{
	TArray<FMeshImageInfo> Out;

	const UMeshDef* Def = MeshForgeToolsetPrivate::RequireDef(DefinitionPath);
	if (Def == nullptr)
	{
		return Out;
	}

	for (const TSoftObjectPtr<UTexture2D>& Image : Def->GatherImagePool())
	{
		Out.Add(MeshForgePipelineToolsPrivate::Describe(Def, Image));
	}

	return Out;
}

FMeshImageSelection UMeshForgeToolset::ChooseImages(
	const FString& DefinitionPath, const FString& MainImagePath, const TArray<FString>& ExtraViewPaths)
{
	FMeshImageSelection Selection;

	UMeshDef* Def = MeshForgeToolsetPrivate::RequireDef(DefinitionPath);
	if (Def == nullptr)
	{
		return Selection;
	}

	const TArray<TSoftObjectPtr<UTexture2D>> Pool = Def->GatherImagePool();

	auto Find = [&Pool](const FString& Path) -> TSoftObjectPtr<UTexture2D>
	{
		for (const TSoftObjectPtr<UTexture2D>& Image : Pool)
		{
			// Both the full object path and the bare package path, because an agent that got this
			// from somewhere other than List Definition Images will have one or the other.
			if (Image.ToString() == Path || Image.ToString().StartsWith(Path + TEXT(".")))
			{
				return Image;
			}
		}

		return TSoftObjectPtr<UTexture2D>();
	};

	Def->Modify();

	if (!MainImagePath.IsEmpty())
	{
		const TSoftObjectPtr<UTexture2D> Chosen = Find(MainImagePath);

		if (Chosen.IsNull())
		{
			UKismetSystemLibrary::RaiseScriptError(FString::Printf(
				TEXT("%s is not one of this definition's pictures. Call List Definition Images for "
					 "the ones it has, or add it to the gallery first."), *MainImagePath));
			return Selection;
		}

		Def->MainImage = Chosen;
		Def->ExtraViews.Remove(Chosen);
	}

	if (ExtraViewPaths.Num() > 0)
	{
		Def->ExtraViews.Reset();

		for (const FString& Path : ExtraViewPaths)
		{
			const TSoftObjectPtr<UTexture2D> View = Find(Path);

			if (View.IsNull())
			{
				UKismetSystemLibrary::RaiseScriptError(FString::Printf(
					TEXT("%s is not one of this definition's pictures. Call List Definition Images "
						 "for the ones it has."), *Path));
				return Selection;
			}

			// A picture cannot be both the subject and one of its own extra angles.
			if (View != Def->ResolveMainImage())
			{
				Def->ExtraViews.AddUnique(View);
			}
		}
	}

	Def->RefreshStaleness();
	Def->MarkPackageDirty();

	Selection.MainImage     = Def->ResolveMainImage().ToString();
	Selection.MaxExtraViews = Def->MaxExtraViews();

	for (const TSoftObjectPtr<UTexture2D>& View : Def->ExtraViews)
	{
		Selection.ExtraViews.Add(View.ToString());
	}

	if (Def->ExtraViews.Num() > Selection.MaxExtraViews)
	{
		Selection.Error = FString::Printf(
			TEXT("This definition's generator uses %d extra view(s); the other %d stay on the asset "
				 "and are dropped at submit time."),
			Selection.MaxExtraViews, Def->ExtraViews.Num() - Selection.MaxExtraViews);
	}

	return Selection;
}

TArray<FMeshForgeJob> UMeshForgeToolset::ListJobs()
{
	UMeshForgeSubsystem* Subsystem = MeshForgeToolsetPrivate::RequireSubsystem();
	return Subsystem ? Subsystem->GetJobs() : TArray<FMeshForgeJob>();
}
