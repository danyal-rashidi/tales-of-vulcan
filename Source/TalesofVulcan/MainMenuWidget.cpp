#include "MainMenuWidget.h"
#include "GameAudio.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "Styling/CoreStyle.h"

namespace MenuLook
{
	const FLinearColor Parchment(0.93f, 0.84f, 0.68f);
	const FLinearColor Idle(0.62f, 0.57f, 0.5f);
	const FLinearColor Ember(1.f, 0.55f, 0.22f);
	const TCHAR* Labels[] = { TEXT("New Game"), TEXT("Quit") };
}

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

		// Title on the right, in the dark beside Elvis, with a thin ember-coloured rule under it.
		Title = Text(TEXT("Tales of Vulcan"), Font(78, true, 60), MenuLook::Parchment, TEXT("Title"));
		Place(Title, FVector2D(0.72f, 0.3f), FVector2D(0.5f, 0.5f));
		TitleRule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TitleRule"));
		TitleRule->SetBrushColor(FLinearColor(1.f, 0.45f, 0.15f, 0.7f));
		UCanvasPanelSlot* RuleSlot = Place(TitleRule, FVector2D(0.72f, 0.3f), FVector2D(0.5f, 0.5f), FVector2D(0.f, 62.f));
		RuleSlot->SetAutoSize(false);
		RuleSlot->SetSize(FVector2D(420.f, 2.f));

		Prompt = Text(TEXT("Press any button"), Font(22, false, 350), MenuLook::Parchment, TEXT("Prompt"));
		Place(Prompt, FVector2D(0.72f, 0.62f), FVector2D(0.5f, 0.5f));

		Choices = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Choices"));
		Place(Choices, FVector2D(0.72f, 0.58f), FVector2D(0.5f, 0.f));
		for (int32 i = 0; i < UE_ARRAY_COUNT(MenuLook::Labels); ++i)
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
			if (i == 0)
			{
				Button->OnHovered.AddDynamic(this, &UMainMenuWidget::HandleNewGameHovered);
			}
			else
			{
				Button->OnHovered.AddDynamic(this, &UMainMenuWidget::HandleQuitHovered);
			}
			Button->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleClicked);

			UTextBlock* Label = Text(FString(MenuLook::Labels[i]).ToUpper(), Font(30, false, 250), MenuLook::Idle, NAME_None);
			Label->SetJustification(ETextJustify::Center);
			Button->AddChild(Label);
			UVerticalBoxSlot* ChoiceSlot = Choices->AddChildToVerticalBox(Button);
			ChoiceSlot->SetHorizontalAlignment(HAlign_Center);
			ChoiceSlot->SetPadding(FMargin(0.f, 10.f));
			ChoiceLabels.Add(Label);
		}

		// Full-screen black for the fades.
		Blackout = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Blackout"));
		Blackout->SetBrushColor(FLinearColor::Black);
		Blackout->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanelSlot* BlackSlot = Canvas->AddChildToCanvas(Blackout);
		BlackSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BlackSlot->SetOffsets(FMargin(0.f));
	}
	return Super::RebuildWidget();
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	State = EState::Intro;
	StateTime = 0.f;
	Title->SetRenderOpacity(0.f);
	TitleRule->SetRenderOpacity(0.f);
	Prompt->SetRenderOpacity(0.f);
	Choices->SetRenderOpacity(0.f);
	Choices->SetVisibility(ESlateVisibility::Hidden);
	Select(0);
}

void UMainMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;
	StateTime += InDeltaTime;

	switch (State)
	{
	case EState::Intro:
	{
		// Out of black over 2.5 s, the title rising in after it.
		Blackout->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 1.f - FMath::SmoothStep(0.f, 2.5f, StateTime)));
		const float TitleIn = FMath::SmoothStep(1.2f, 3.2f, StateTime);
		Title->SetRenderOpacity(TitleIn);
		TitleRule->SetRenderOpacity(TitleIn);
		if (StateTime > 3.2f)
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
		const float In = FMath::SmoothStep(0.f, 0.6f, StateTime);
		Prompt->SetRenderOpacity((1.f - In) * 0.8f);
		Choices->SetRenderOpacity(In);
		// The chosen entry glows like an ember.
		if (ChoiceLabels.IsValidIndex(Selected))
		{
			const float Glow = 0.85f + 0.15f * FMath::Sin(Time * 3.f);
			ChoiceLabels[Selected]->SetColorAndOpacity(FSlateColor(MenuLook::Ember * Glow));
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
	}
}

void UMainMenuWidget::OpenChoices()
{
	if (State == EState::Choices || State == EState::Leaving)
	{
		return;
	}
	Blackout->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
	Title->SetRenderOpacity(1.f);
	TitleRule->SetRenderOpacity(1.f);
	Choices->SetVisibility(ESlateVisibility::Visible);
	State = EState::Choices;
	StateTime = 0.f;
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

void UMainMenuWidget::Select(int32 Index)
{
	Selected = (Index + ChoiceLabels.Num()) % FMath::Max(ChoiceLabels.Num(), 1);
	for (int32 i = 0; i < ChoiceLabels.Num(); ++i)
	{
		const FString Label = FString(MenuLook::Labels[i]).ToUpper();
		ChoiceLabels[i]->SetText(FText::FromString(i == Selected ? FString::Printf(TEXT("\u2014  %s  \u2014"), *Label) : Label));
		ChoiceLabels[i]->SetColorAndOpacity(FSlateColor(i == Selected ? MenuLook::Ember : MenuLook::Idle));
	}
}

void UMainMenuWidget::Activate()
{
	if (State != EState::Choices)
	{
		return;
	}
	GameAudio::Play2D(this, TEXT("Bell"), 0.6f);
	if (Selected == 0)
	{
		OnNewGame.ExecuteIfBound();
	}
	else
	{
		OnQuit.ExecuteIfBound();
	}
}

FReply UMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (State == EState::Intro || State == EState::PressAny)
	{
		OpenChoices();
		return FReply::Handled();
	}
	if (State != EState::Choices)
	{
		return FReply::Handled();
	}
	if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
	{
		Select(Selected - 1);
		GameAudio::Play2D(this, TEXT("Roll"), 0.25f, 1.4f);
	}
	else if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
	{
		Select(Selected + 1);
		GameAudio::Play2D(this, TEXT("Roll"), 0.25f, 1.4f);
	}
	else if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		Activate();
	}
	return FReply::Handled();
}

FReply UMainMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (State == EState::Intro || State == EState::PressAny)
	{
		OpenChoices();
	}
	return FReply::Handled();
}

void UMainMenuWidget::HandleNewGameHovered()
{
	if (State == EState::Choices && Selected != 0)
	{
		Select(0);
	}
}

void UMainMenuWidget::HandleQuitHovered()
{
	if (State == EState::Choices && Selected != 1)
	{
		Select(1);
	}
}

void UMainMenuWidget::HandleClicked()
{
	Activate();
}
