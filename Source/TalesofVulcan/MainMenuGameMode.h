#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainMenuGameMode.generated.h"

class ACameraActor;
class UAudioComponent;
class USoundBase;
class UMainMenuWidget;

/**
 * The main menu (/Game/Menu/L_MainMenu, the game's start map). The level itself is empty: this game mode builds
 * the scene when play starts. Elvis stands in a dark, foggy Roman ruin between two braziers and a bonfire
 * (AFlameFX), embers drifting, with Vulcan's bronze statue looming behind in the fog. The camera drifts slowly,
 * and "Wrath of Vulkan" (S_MenuMusic_01..03 in /Game/Audio) plays in a loop. UMainMenuWidget is the screen on top.
 * New Game opens the arena (Lvl_ThirdPerson). A test flag (-EnvShot, -OtterShot, ...) skips straight to the arena.
 * -MenuShot films the menu into Saved/SpearShots, then picks New Game and lets the arena's -EnvShot film and quit.
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
	void SpawnBonfire(const FVector& Location);
	void StartMusic();
	void StartNewGame();
	void Quit();
	void StartMenuShot();

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

	FVector CameraBase = FVector::ZeroVector;
	FVector CameraLookAt = FVector::ZeroVector;
	float Time = 0.f;
	int32 Song = 0;
	bool bLeaving = false;
};
