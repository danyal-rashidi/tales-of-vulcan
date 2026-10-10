// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class TalesofVulcan : ModuleRules
{
	public TalesofVulcan(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule", "GameplayTasks", "UMG", "Slate", "SlateCore", "EngineCameras" });

		PrivateDependencyModuleNames.AddRange(new string[] { "AudioMixer" }); // GroundCheckSubsystem records the game audio in test runs

		PrivateDependencyModuleNames.Add("IKRig"); // ElvisAnimInstance retargets Quinn's pose onto Elvis
		PrivateDependencyModuleNames.Add("AnimGraphRuntime"); // VulcanAnimInstance plays montages through a slot node
		PrivateDependencyModuleNames.Add("Landscape"); // RomeMapTools builds the Rome map's landscape (editor only)

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
