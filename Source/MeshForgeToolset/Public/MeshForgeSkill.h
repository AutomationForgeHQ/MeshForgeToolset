// What an agent needs to know about generating meshes that no tool signature can say.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/AgentSkill.h"
#include "MeshForgeSkill.generated.h"

/**
 * Ordering, cost and the gotchas of generated meshes.
 *
 * Native, so it is discovered automatically - no registration. Deliberately says nothing that a tool
 * description already says; what is here is the knowledge that has no signature to live in.
 */
UCLASS()
class MESHFORGETOOLSET_API UMeshForgeSkill : public UAgentSkill
{
	GENERATED_BODY()

public:

	UMeshForgeSkill();
};
