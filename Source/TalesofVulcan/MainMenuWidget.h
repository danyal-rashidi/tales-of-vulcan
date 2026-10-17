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
 * Cinzel (/Game/Menu/Fonts, SIL Open Font License), waits for any button, then offers Continue (greyed until there is
 * a save system), New Game, Options and Quit. Options holds volume, graphics quality and display mode, saved in
 * GameUserSettings.ini. New Game shows the prologue card (the story's opening lines) before the game loads.
 * Keyboard (arrows / WASD, Enter / Space, Esc), gamepad (d-pad / stick, A, B) and mouse all work.
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

	/** Show the options page (used by the -MenuShot test). */
	void OpenOptions();

	/** Back to the main choices (used by the -MenuShot test). */
	void CloseOptions();

	/** Fade to black, then run Then. */
	void FadeOut(float Seconds, TFunction<void()> Then);

	/** Fade to black, show the prologue card, then run Then (any button skips the card). */
	void PlayPrologue(TFunction<void()> Then);

	/** The saved master volume (0..1), applied to the audio device. Called when the menu starts. */
	static void ApplySavedVolume();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	enum class EState : uint8 { Intro, PressAny, Choices, Leaving, Prologue };
	enum class EPage : uint8 { Main, Options };

	FSlateFontInfo Font(int32 Size, bool bBold, int32 LetterSpacing = 0) const;
	void ShowPage(EPage NewPage);
	void Refresh();
	void Select(int32 Index, bool bSound = true);
	void Step(int32 Delta);
	void Change(int32 Delta);
	void Activate();
	void Back();
	FString RowText(int32 Row) const;
	bool RowEnabled(int32 Row) const;
	int32 RowCount() const;
	void LoadOptions();
	void ApplyOption(int32 Row);

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
	TObjectPtr<UTextBlock> PageTitle;
	UPROPERTY()
	TObjectPtr<UVerticalBox> Choices;
	UPROPERTY()
	TArray<TObjectPtr<UButton>> RowButtons;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> RowLabels;
	UPROPERTY()
	TObjectPtr<UTextBlock> Hint;
	UPROPERTY()
	TObjectPtr<UBorder> Blackout;
	UPROPERTY()
	TObjectPtr<UVerticalBox> Card;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> CardLines;

	EState State = EState::Intro;
	EPage Page = EPage::Main;
	float StateTime = 0.f;
	float Time = 0.f;
	int32 Selected = 1;
	float LeaveSeconds = 1.f;
	TFunction<void()> AfterFade;

	// Options, applied as they change and saved to GameUserSettings.ini.
	int32 Volume = 8;  // 0..10
	int32 Quality = 2; // 0 low .. 3 epic
	int32 Display = 1; // 0 windowed, 1 borderless, 2 fullscreen
};
