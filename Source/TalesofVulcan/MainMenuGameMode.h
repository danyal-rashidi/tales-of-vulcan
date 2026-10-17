#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainMenuGameMode.generated.h"

class ACameraActor;
class UAudioComponent;
class UDirectionalLightComponent;
class UPointLightComponent;
class USkeletalMeshComponent;
class USoundBase;
class UMainMenuWidget;

/**
 * The main menu (/Game/Menu/L_MainMenu, the game's start map). The level itself is empty: this game mode builds
 * the scene when play starts, from the story's opening (story doc, "Prologue"): inside the lone colosseum, at night,
 * under Vulcan's blood-red storm, the bronze statue of the possessed god stands on its plinth with the crown on his
 * head glowing and shedding embers; Elvis, the stranger who will wake him, stands in the foreground facing it.
 * A red sun over the far wall, blood rain, lightning with thunder, braziers round the plinth, toppled columns.
 * The camera drifts slowly, and "Wrath of Vulkan" (S_MenuMusic_01..03 in /Game/Audio) plays in a loop.
 * UMainMenuWidget is the screen on top. New Game plays the prologue card, then opens the Rome map (L_Rome).
 * A test flag (-EnvShot, -OtterShot, ...) skips straight to the arena level (Lvl_ThirdPerson).
 * -MenuShot films the menu (title, choices, options, the prologue card) into Saved/SpearShots, then starts a New Game
 * and lets the game's -EnvShot film and quit.
 */
UCLASS()
class TALESOFVULCAN_API AMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMainMenuGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void BuildScene();
	void SpawnElvis(const FVector& Location, float Yaw);
	void SpawnStatue(const FVector& Location, float Yaw, float Scale);
	void SpawnBrazier(const FVector& Location);
	void UpdateStorm(float DeltaSeconds);
	void StartMusic();
	void StartNewGame();
	void Quit();
	void StartMenuShot();
	FVector CrownLocation() const;

	UFUNCTION()
	void HandleSongFinished();

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Music;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> Songs;

	UPROPERTY(Transient)
	TObjectPtr<UMainMenuWidget> Menu;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Set;

	UPROPERTY(Transient)
	TObjectPtr<class ACharacter> Elvis;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Statue;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> CrownLight;

	UPROPERTY(Transient)
	TObjectPtr<UDirectionalLightComponent> Lightning;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CrownEmbers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UStaticMeshComponent>> CrownSpikes;

	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> CrownMaterial;

	FVector CameraBase = FVector::ZeroVector;
	FVector CameraLookAt = FVector::ZeroVector;
	float Time = 0.f;
	float NextFlash = 6.f;   // when the next lightning strikes
	float FlashTime = -10.f; // when the last one struck
	float ThunderAt = -1.f;  // when its thunder arrives
	int32 Song = 0;
	bool bLeaving = false;
};
