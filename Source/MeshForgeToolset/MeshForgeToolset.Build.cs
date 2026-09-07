using UnrealBuildTool;

public class MeshForgeToolset : ModuleRules
{
	public MeshForgeToolset(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"MeshForge",        // the capability this adapts
				"ToolsetRegistry",  // UToolsetDefinition, UAgentSkill, UToolCallAsyncResult
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"AssetRegistry",    // finding mesh definitions by class
			}
			);
	}
}
