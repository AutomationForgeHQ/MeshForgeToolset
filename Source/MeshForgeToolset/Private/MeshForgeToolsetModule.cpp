#include "MeshForgeToolset.h"

#include "Modules/ModuleManager.h"

/**
 * Registers MeshForge's toolset.
 *
 * Registration is explicit and there is exactly one line per toolset: a class that is written,
 * compiles and is never named here simply does not exist as far as an agent is concerned, with no
 * warning anywhere.
 *
 * The skill needs no registration - native UAgentSkill subclasses are discovered by the registry.
 */
class FMeshForgeToolsetModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
		// Guarded because ToolsetRegistry is an Experimental engine plugin and this module can load
		// in a project where it is switched off. Registering into a registry that is not there is
		// the only failure mode this plugin has.
		if (UToolsetRegistry::IsAvailable())
		{
			UToolsetRegistry::RegisterToolsetClass(UMeshForgeToolset::StaticClass());
		}
	}

	virtual void ShutdownModule() override
	{
		if (UToolsetRegistry::IsAvailable())
		{
			UToolsetRegistry::UnregisterToolsetClass(UMeshForgeToolset::StaticClass());
		}
	}
};

IMPLEMENT_MODULE(FMeshForgeToolsetModule, MeshForgeToolset)
