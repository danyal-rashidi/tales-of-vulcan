#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UBorder;
class UButton;
class UFontFace;
class UTextBlock;
class UVerticalBox;
struct FCompositeFont;

/**
 * The title screen, built in C++ (no widget Blueprint): fades in from black, shows "Tales of Vulcan" in
 * Cinzel (/Game/Menu/Fonts, SIL Open Font License), waits for any button, then offers New Game and Quit.
 * Keyboard (arrows / WASD, Enter / Space), gamepad (d-pad / stick, face button) and mouse all work.
 * Created by AMainMenuGameMode, which it calls back to start the game or quit.
 */
UCLASS()
class TALESOFVULCAN_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE(FOnChoice);
	FOnChoice OnNewGame;
	FOnChoice OnQuit;

	/** Leave "press any button" and show the choices (also used by the -MenuShot test). */
	void OpenChoices();

	/** Fade to black, then run Then. */
	void FadeOut(float Seconds, TFunction<void()> Then);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	enum class EState : uint8 { Intro, PressAny, Choices, Leaving };

	FSlateFontInfo Font(int32 Size, bool bBold, int32 LetterSpacing = 0) const;
	void Select(int32 Index);
	void Activate();

	UFUNCTION()
	void HandleNewGameHovered();
	UFUNCTION()
	void HandleQuitHovered();
	UFUNCTION()
	void HandleClicked();

	UPROPERTY()
	TObjectPtr<UFontFace> RegularFace;
	UPROPERTY()
	TObjectPtr<UFontFace> BoldFace;
	TSharedPtr<FCompositeFont> RegularFont;
	TSharedPtr<FCompositeFont> BoldFont;

	UPROPERTY()
	TObjectPtr<UTextBlock> Title;
	UPROPERTY()
	TObjectPtr<UBorder> TitleRule;
	UPROPERTY()
	TObjectPtr<UTextBlock> Prompt;
	UPROPERTY()
	TObjectPtr<UVerticalBox> Choices;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> ChoiceLabels;
	UPROPERTY()
	TObjectPtr<UBorder> Blackout;

	EState State = EState::Intro;
	float StateTime = 0.f;
	float Time = 0.f;
	int32 Selected = 0;
	float LeaveSeconds = 1.f;
	TFunction<void()> AfterFade;
};
