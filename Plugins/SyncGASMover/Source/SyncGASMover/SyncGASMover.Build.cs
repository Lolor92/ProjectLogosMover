// Copyright 2026 WeirdReflection. All Rights Reserved.

using UnrealBuildTool;

public class SyncGASMover : ModuleRules
{
    public SyncGASMover(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "GameplayAbilities",
                "GameplayTags",
                "GameplayTasks",
                "Mover",
                "MotionWarping"
            });

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine"
            });
    }
}
