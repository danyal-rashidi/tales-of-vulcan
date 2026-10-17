#include "MainMenuWidget.h"
#include "GameAudio.h"
#include "AudioDevice.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/CoreStyle.h"

namespace MenuLook
{
	const FLinearColor Parchment(0.93f, 0.84f, 0.68f);
	const FLinearColor Idle(0.62f, 0.57f, 0.5f);
	const FLinearColor Locked(0.3f, 0.28f, 0.26f);
	const FLinearColor Ember(1.f, 0.55f, 0.22f);
	const FLinearColor Crown(1.f, 0.22f, 0.06f);
	constexpr int32 Rows = 4;
	constexpr float Column = 0.27f; // the menu's column, a fifth across the screen, in the dark wall left of the statue

	const TCHAR* MainRows[Rows] = { TEXT("Continue"), TEXT("New Game"), TEXT("Options"), TEXT("Quit") };
	const TCHAR* Qualities[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") };
	const TCHAR* Displays[] = { TEXT("Windowed"), TEXT("Borderless"), TEXT("Fullscreen") };

	// The prologue card: the story's opening, in its own words (story doc, "Prologue").
	const TCHAR* Card[] = {
		TEXT("Prologue"),
		TEXT("A colosseum stands alone in the desert."),
		TEXT("Inside, the god of fire sleeps in bronze,"),
		TEXT("wearing a crown that is not his.") };
	constexpr float CardLineStart = 1.7f, CardLineGap = 1.5f, CardHold = 9.2f, CardEnd = 10.4f;

	const TCHAR* Section = TEXT("TalesOfVulcan.Options");
}

// ---------------------------------------------------------------- options storage

void UMainMenuWidget::ApplySavedVolume()
{
	int32 Saved = 8;
	GConfig->GetInt(MenuLook::Section, TEXT("Volume"), Saved, GGameUserSettingsIni);
	if (GEngine)
	{
		if (FAudioDeviceHandle Device = GEngine->GetMainAudioDevice())
		{
			Device->SetTransientPrimaryVolume(FMath::Clamp(Saved, 0, 10) / 10.f);
		}
	}
}

void UMainMenuWidget::LoadOptions()
{
	GConfig->GetInt(MenuLook::Section, TEXT("Volume"), Volume, GGameUserSettingsIni);
	Volume = FMath::Clamp(Volume, 0, 10);
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		const int32 Level = Settings->GetOverallScalabilityLevel(); // -1 when the levels are mixed
		Quality = Level >= 0 ? FMath::Clamp(Level, 0, 3) : 2;
		switch (Settings->GetFullscreenMode())
		{
		case EWindowMode::Windowed: Display = 0; break;
		case EWindowMode::WindowedFullscreen: Display = 1; break;
		default: Display = 2; break;
		}
	}
}

void UMainMenuWidget::ApplyOption(int32 Row)
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (Row == 0)
	{
		GConfig->SetInt(MenuLook::Section, TEXT("Volume"), Volume, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		ApplySavedVolume();
	}
	else if (Row == 1 && Settings)
	{
		Settings->SetOverallScalabilityLevel(Quality);
		Settings->ApplyNonResolutionSettings();
		Settings->SaveSettings();
	}
	else if (Row == 2 && Settings)
	{
		const EWindowMode::Type Modes[] = { EWindowMode::Windowed, EWindowMode::WindowedFullscreen, EWindowMode::Fullscreen };
		Settings->SetFullscreenMode(Modes[Display]);
		if (!GIsEditor) // the editor's play window keeps its own mode
		{
			Settings->ApplyResolutionSettings(false);
		}
		Settings->SaveSettings();
	}
}

// ---------------------------------------------------------------- building

FSlateFontInfo UMainMenuWidget::Font(int32 Size, bool bBold, int32 LetterSpacing) const
{
	const TSharedPtr<FCompositeFont>& Composite = bBold ? BoldFont : RegularFont;
	FSlateFontInfo Info = Composite.IsValid() ? FSlateFontInfo(Composite, Size) : FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	Info.LetterSpacing = LetterSpacing;
	return Info;
}

TSharedRef<SWidget> UMainMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		// Cinzel, built into a font at runtime from its font face assets.
		auto MakeFont = [](UFontFace* Face)
		{
			TSharedPtr<FCompositeFont> Composite;
			if (Face)
			{
				Composite = MakeShared<FCompositeFont>();
				FTypefaceEntry& Entry = Composite->DefaultTypeface.Fonts.AddDefaulted_GetRef();
				Entry.Name = TEXT("Regular");
				Entry.Font = FFontData(Face);
			}
			return Composite;
		};
		RegularFace = LoadObject<UFontFace>(nullptr, TEXT("/Game/Menu/Fonts/Cinzel-Regular.Cinzel-Regular"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		BoldFace = LoadObject<UFontFace>(nullptr, TEXT("/Game/Menu/Fonts/Cinzel-Bold.Cinzel-Bold"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		RegularFont = MakeFont(RegularFace);
		BoldFont = MakeFont(BoldFace);

		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
		WidgetTree->RootWidget = Canvas;

		auto Text = [&](const FString& String, const FSlateFontInfo& FontInfo, const FLinearColor& Color, const FName Name)
		{
			UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
			Block->SetText(FText::FromString(String));
			Block->SetFont(FontInfo);
			Block->SetColorAndOpacity(FSlateColor(Color));
			Block->SetShadowOffset(FVector2D(2.f, 3.f));
			Block->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
			return Block;
		};
		auto Place = [&](UWidget* Widget, const FVector2D& Anchor, const FVector2D& Alignment, const FVector2D& Offset = FVector2D::ZeroVector)
		{
			UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Widget);
			CanvasSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
			CanvasSlot->SetAlignment(Alignment);
			CanvasSlot->SetPosition(Offset);
			CanvasSlot->SetAutoSize(true);
			return CanvasSlot;
		};
		const float X = MenuLook::Column;

		// The title in the dark beside the statue, an ember rule under it.
		Title = Text(TEXT("Tales of Vulcan"), Font(74, true, 60), MenuLook::Parchment, TEXT("Title"));
		Place(Title, FVector2D(X, 0.24f), FVector2D(0.5f, 0.5f));
		TitleRule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TitleRule"));
		TitleRule->SetBrushColor(FLinearColor(1.f, 0.35f, 0.1f, 0.75f));
		UCanvasPanelSlot* RuleSlot = Place(TitleRule, FVector2D(X, 0.24f), FVector2D(0.5f, 0.5f), FVector2D(0.f, 60.f));
		RuleSlot->SetAutoSize(false);
		RuleSlot->SetSize(FVector2D(380.f, 2.f));

		Prompt = Text(TEXT("Press any button"), Font(20, false, 350), MenuLook::Parchment, TEXT("Prompt"));
		Place(Prompt, FVector2D(X, 0.62f), FVector2D(0.5f, 0.5f));

		PageTitle = Text(TEXT("Options"), Font(20, true, 400), MenuLook::Ember, TEXT("PageTitle"));
		Place(PageTitle, FVector2D(X, 0.44f), FVector2D(0.5f, 0.5f));

		Choices = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Choices"));
		Place(Choices, FVector2D(X, 0.49f), FVector2D(0.5f, 0.f));
		for (int32 i = 0; i < MenuLook::Rows; ++i)
		{
			// A borderless button around each label, for the mouse; keys are handled by the widget itself.
			UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			FButtonStyle Style = Button->GetStyle();
			for (FSlateBrush* Brush : { &Style.Normal, &Style.Hovered, &Style.Pressed, &Style.Disabled })
			{
				Brush->DrawAs = ESlateBrushDrawType::NoDrawType;
			}
			Style.NormalPadding = Style.PressedPadding = FMargin(0.f);
			Button->SetStyle(Style);
			Button->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleClicked);

			UTextBlock* Label = Text(TEXT(""), Font(28, false, 250), MenuLook::Idle, NAME_None);
			Label->SetJustification(ETextJustify::Center);
			Button->AddChild(Label);
			UVerticalBoxSlot* RowSlot = Choices->AddChildToVerticalBox(Button);
			RowSlot->SetHorizontalAlignment(HAlign_Center);
			RowSlot->SetPadding(FMargin(0.f, 9.f));
			RowButtons.Add(Button);
			RowLabels.Add(Label);
		}

		Hint = Text(TEXT(""), Font(13, false, 200), MenuLook::Idle, TEXT("Hint"));
		Place(Hint, FVector2D(X, 0.93f), FVector2D(0.5f, 0.5f));

		// Full-screen black for the fades, and the prologue card on top of it.
		Blackout = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Blackout"));
		Blackout->SetBrushColor(FLinearColor::Black);
		Blackout->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanelSlot* BlackSlot = Canvas->AddChildToCanvas(Blackout);
		BlackSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BlackSlot->SetOffsets(FMargin(0.f));

		Card = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Card"));
		Place(Card, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f));
		for (int32 i = 0; i < UE_ARRAY_COUNT(MenuLook::Card); ++i)
		{
			const bool bHeading = i == 0;
			UTextBlock* Line = Text(bHeading ? FString(MenuLook::Card[i]).ToUpper() : FString(MenuLook::Card[i]),
				bHeading ? Font(22, true, 500) : Font(30, false, 40), bHeading ? MenuLook::Ember : MenuLook::Parchment, NAME_None);
			Line->SetJustification(ETextJustify::Center);
			UVerticalBoxSlot* LineSlot = Card->AddChildToVerticalBox(Line);
			LineSlot->SetHorizontalAlignment(HAlign_Center);
			LineSlot->SetPadding(FMargin(0.f, bHeading ? 0.f : 8.f, 0.f, bHeading ? 34.f : 8.f));
			CardLines.Add(Line);
		}
		Card->SetVisibility(ESlateVisibility::Collapsed);
	}
	return Super::RebuildWidget();
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	LoadOptions();
	State = EState::Intro;
	StateTime = 0.f;
	Title->SetRenderOpacity(0.f);
	TitleRule->SetRenderOpacity(0.f);
	Prompt->SetRenderOpacity(0.f);
	Hint->SetRenderOpacity(0.f);
	Choices->SetRenderOpacity(0.f);
	Choices->SetVisibility(ESlateVisibility::Hidden);
	PageTitle->SetVisibility(ESlateVisibility::Hidden);
	ShowPage(EPage::Main);
}

// ---------------------------------------------------------------- pages and rows

int32 UMainMenuWidget::RowCount() const
{
	return MenuLook::Rows;
}

bool UMainMenuWidget::RowEnabled(int32 Row) const
{
	return !(Page == EPage::Main && Row == 0); // Continue waits for a save system
}

FString UMainMenuWidget::RowText(int32 Row) const
{
	if (Page == EPage::Main)
	{
		return FString(MenuLook::MainRows[Row]).ToUpper();
	}
	switch (Row)
	{
	case 0: return FString::Printf(TEXT("VOLUME   <  %d  >"), Volume * 10);
	case 1: return FString::Printf(TEXT("GRAPHICS   <  %s  >"), *FString(MenuLook::Qualities[Quality]).ToUpper());
	case 2: return FString::Printf(TEXT("DISPLAY   <  %s  >"), *FString(MenuLook::Displays[Display]).ToUpper());
	default: return TEXT("BACK");
	}
}

void UMainMenuWidget::ShowPage(EPage NewPage)
{
	Page = NewPage;
	PageTitle->SetVisibility(Page == EPage::Options && State == EState::Choices ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	Hint->SetText(FText::FromString(Page == EPage::Main
		? TEXT("W / S  CHOOSE      ENTER  SELECT")
		: TEXT("W / S  CHOOSE      A / D  CHANGE      ESC  BACK")));
	Selected = Page == EPage::Main ? 1 : 0;
	Refresh();
}

void UMainMenuWidget::Refresh()
{
	for (int32 i = 0; i < RowLabels.Num(); ++i)
	{
		const bool bOn = i == Selected;
		const FString Label = RowText(i);
		RowLabels[i]->SetText(FText::FromString(bOn ? FString::Printf(TEXT("—  %s  —"), *Label) : Label));
		RowLabels[i]->SetColorAndOpacity(FSlateColor(!RowEnabled(i) ? MenuLook::Locked : bOn ? MenuLook::Ember : MenuLook::Idle));
	}
}

void UMainMenuWidget::Select(int32 Index, bool bSound)
{
	const int32 Count = RowCount();
	Index = (Index % Count + Count) % Count;
	if (!RowEnabled(Index) || Index == Selected)
	{
		return;
	}
	Selected = Index;
	Refresh();
	if (bSound)
	{
		GameAudio::Play2D(this, TEXT("Roll"), 0.25f, 1.4f);
	}
}

void UMainMenuWidget::Step(int32 Delta)
{
	// Move to the next row that can be chosen, skipping greyed ones.
	const int32 Count = RowCount();
	for (int32 Try = 1; Try <= Count; ++Try)
	{
		const int32 Index = ((Selected + Delta * Try) % Count + Count) % Count;
		if (RowEnabled(Index))
		{
			Select(Index);
			return;
		}
	}
}

void UMainMenuWidget::Change(int32 Delta)
{
	if (Page != EPage::Options || Selected > 2)
	{
		return;
	}
	if (Selected == 0) { Volume = FMath::Clamp(Volume + Delta, 0, 10); }
	if (Selected == 1) { Quality = (Quality + Delta + 4) % 4; }
	if (Selected == 2) { Display = (Display + Delta + 3) % 3; }
	ApplyOption(Selected);
	Refresh();
	GameAudio::Play2D(this, TEXT("LockOn"), 0.35f, 1.2f);
}

void UMainMenuWidget::Activate()
{
	if (State != EState::Choices || !RowEnabled(Selected))
	{
		return;
	}
	if (Page == EPage::Options)
	{
		if (Selected == 3) { Back(); }
		else { Change(+1); }
		return;
	}
	switch (Selected)
	{
	case 1: GameAudio::Play2D(this, TEXT("Bell"), 0.6f); OnNewGame.ExecuteIfBound(); break;
	case 2: GameAudio::Play2D(this, TEXT("LockOn"), 0.5f, 0.9f); OpenOptions(); break;
	case 3: GameAudio::Play2D(this, TEXT("Bell"), 0.4f, 0.8f); OnQuit.ExecuteIfBound(); break;
	default: break;
	}
}

void UMainMenuWidget::Back()
{
	if (State == EState::Choices && Page == EPage::Options)
	{
		CloseOptions();
		GameAudio::Play2D(this, TEXT("Roll"), 0.3f, 1.1f);
	}
}

void UMainMenuWidget::OpenOptions()
{
	OpenChoices();
	ShowPage(EPage::Options);
	StateTime = 0.f;
}

void UMainMenuWidget::CloseOptions()
{
	ShowPage(EPage::Main);
	Select(2, false); // back on Options
	StateTime = 0.f;
}

// ---------------------------------------------------------------- states

void UMainMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;
	StateTime += InDeltaTime;

	switch (State)
	{
	case EState::Intro:
	{
		// Out of black over 3 s, the title rising in after it.
		Blackout->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 1.f - FMath::SmoothStep(0.f, 3.f, StateTime)));
		const float TitleIn = FMath::SmoothStep(1.5f, 3.6f, StateTime);
		Title->SetRenderOpacity(TitleIn);
		TitleRule->SetRenderOpacity(TitleIn);
		if (StateTime > 3.6f)
		{
			State = EState::PressAny;
			StateTime = 0.f;
		}
		break;
	}
	case EState::PressAny:
		// A slow breathing pulse.
		Prompt->SetRenderOpacity(FMath::Min(StateTime / 0.8f, 1.f) * (0.45f + 0.4f * (0.5f + 0.5f * FMath::Sin(Time * 2.2f))));
		break;
	case EState::Choices:
	{
		const float In = FMath::SmoothStep(0.f, 0.5f, StateTime);
		Prompt->SetRenderOpacity(FMath::Max(Prompt->GetRenderOpacity() - InDeltaTime * 2.f, 0.f));
		Choices->SetRenderOpacity(In);
		PageTitle->SetRenderOpacity(In);
		Hint->SetRenderOpacity(0.7f * In);
		// The mouse picks the row it is over.
		for (int32 i = 0; i < RowButtons.Num(); ++i)
		{
			if (RowButtons[i]->IsHovered() && i != Selected && RowEnabled(i))
			{
				Select(i);
			}
		}
		// The chosen row glows like the crown's embers.
		if (RowLabels.IsValidIndex(Selected))
		{
			const float Glow = 0.85f + 0.15f * FMath::Sin(Time * 3.f);
			RowLabels[Selected]->SetColorAndOpacity(FSlateColor(MenuLook::Ember * Glow));
		}
		break;
	}
	case EState::Leaving:
	{
		const float Out = FMath::Clamp(StateTime / LeaveSeconds, 0.f, 1.f);
		Blackout->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, Out));
		if (Out >= 1.f && AfterFade)
		{
			TFunction<void()> Then = MoveTemp(AfterFade);
			AfterFade = nullptr;
			Then();
		}
		break;
	}
	case EState::Prologue:
	{
		// Fade to black, then the card's lines one after another, a hold, and out.
		Blackout->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, FMath::SmoothStep(0.f, 1.3f, StateTime)));
		const float Out = 1.f - FMath::SmoothStep(MenuLook::CardHold, MenuLook::CardEnd, StateTime);
		for (int32 i = 0; i < CardLines.Num(); ++i)
		{
			const float Start = MenuLook::CardLineStart + MenuLook::CardLineGap * i;
			CardLines[i]->SetRenderOpacity(FMath::SmoothStep(Start, Start + 1.1f, StateTime) * Out);
		}
		if (StateTime >= MenuLook::CardEnd + 0.3f && AfterFade)
		{
			TFunction<void()> Then = MoveTemp(AfterFade);
			AfterFade = nullptr;
			Then();
		}
		break;
	}
	}
}

void UMainMenuWidget::OpenChoices()
{
	if (State == EState::Choices || State == EState::Leaving || State == EState::Prologue)
	{
		return;
	}
	Blackout->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
	Title->SetRenderOpacity(1.f);
	TitleRule->SetRenderOpacity(1.f);
	Choices->SetVisibility(ESlateVisibility::Visible);
	State = EState::Choices;
	StateTime = 0.f;
	ShowPage(EPage::Main);
	GameAudio::Play2D(this, TEXT("LockOn"), 0.5f, 0.8f);
}

void UMainMenuWidget::FadeOut(float Seconds, TFunction<void()> Then)
{
	State = EState::Leaving;
	StateTime = 0.f;
	LeaveSeconds = FMath::Max(Seconds, 0.01f);
	AfterFade = MoveTemp(Then);
	Choices->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMainMenuWidget::PlayPrologue(TFunction<void()> Then)
{
	State = EState::Prologue;
	StateTime = 0.f;
	AfterFade = MoveTemp(Then);
	Choices->SetVisibility(ESlateVisibility::HitTestInvisible);
	for (UTextBlock* Line : CardLines)
	{
		Line->SetRenderOpacity(0.f);
	}
	Card->SetVisibility(ESlateVisibility::HitTestInvisible);
}

// ---------------------------------------------------------------- input

FReply UMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (State == EState::Intro || State == EState::PressAny)
	{
		OpenChoices();
		return FReply::Handled();
	}
	if (State == EState::Prologue)
	{
		// Any button skips the card (once the screen is black).
		if (StateTime > 1.3f && StateTime < MenuLook::CardHold)
		{
			StateTime = MenuLook::CardHold;
		}
		return FReply::Handled();
	}
	if (State != EState::Choices)
	{
		return FReply::Handled();
	}
	if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
	{
		Step(-1);
	}
	else if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
	{
		Step(+1);
	}
	else if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
	{
		Change(-1);
	}
	else if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
	{
		Change(+1);
	}
	else if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		Activate();
	}
	else if (Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right)
	{
		Back();
	}
	return FReply::Handled();
}

FReply UMainMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (State == EState::Intro || State == EState::PressAny)
	{
		OpenChoices();
	}
	else if (State == EState::Prologue && StateTime > 1.3f && StateTime < MenuLook::CardHold)
	{
		StateTime = MenuLook::CardHold;
	}
	return FReply::Handled();
}

void UMainMenuWidget::HandleClicked()
{
	Activate();
}
