#include "BossHUDWidget.h"
#include "HealthComponent.h"
#include "VulcanBoss.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace BossHUD
{
	UProgressBar* MakeBar(UWidgetTree* Tree, const FLinearColor& Back, const FLinearColor& Front)
	{
		UProgressBar* Bar = Tree->ConstructWidget<UProgressBar>();
		FProgressBarStyle Style = Bar->GetWidgetStyle();
		Style.SetBackgroundImage(FSlateColorBrush(Back));
		Style.SetFillImage(FSlateColorBrush(FLinearColor::White));
		Bar->SetWidgetStyle(Style);
		Bar->SetFillColorAndOpacity(Front);
		Bar->SetPercent(1.f);
		return Bar;
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const TCHAR* Text, int32 Size, const FLinearColor& Color, int32 LetterSpacing)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
		Block->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Font.LetterSpacing = LetterSpacing;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetShadowOffset(FVector2D(2.f, 2.f));
		Block->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		return Block;
	}
}

void UBossHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	// Covers the whole screen, so it must never catch the mouse.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	// Boss name over a long thin bar, bottom centre. The trail (pale gold) sits behind the red fill.
	UVerticalBox* BarBox = WidgetTree->ConstructWidget<UVerticalBox>();
	BarBox->AddChildToVerticalBox(BossHUD::MakeText(WidgetTree, TEXT("Vulcan, the Silver Tide"), 20, FLinearColor(0.9f, 0.86f, 0.78f), 50))
		->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>();
	BarSize->SetWidthOverride(900.f);
	BarSize->SetHeightOverride(10.f);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
	Trail = BossHUD::MakeBar(WidgetTree, FLinearColor(0.02f, 0.02f, 0.02f, 0.75f), FLinearColor(0.85f, 0.65f, 0.25f));
	Fill = BossHUD::MakeBar(WidgetTree, FLinearColor::Transparent, FLinearColor(0.55f, 0.04f, 0.03f));
	for (UProgressBar* Bar : { Trail.Get(), Fill.Get() })
	{
		UOverlaySlot* Layer = Layers->AddChildToOverlay(Bar);
		Layer->SetHorizontalAlignment(HAlign_Fill);
		Layer->SetVerticalAlignment(VAlign_Fill);
	}
	BarSize->SetContent(Layers);
	BarBox->AddChildToVerticalBox(BarSize);

	UCanvasPanelSlot* BarSlot = Root->AddChildToCanvas(BarBox);
	BarSlot->SetAnchors(FAnchors(0.5f, 1.f));
	BarSlot->SetAlignment(FVector2D(0.5f, 1.f));
	BarSlot->SetPosition(FVector2D(0.f, -70.f));
	BarSlot->SetAutoSize(true);
	BossBar = BarBox;
	BossBar->SetRenderOpacity(0.f);

	// Victory banner: gold text on a dark band across the middle of the screen.
	UBorder* Band = WidgetTree->ConstructWidget<UBorder>();
	Band->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Band->SetPadding(FMargin(0.f, 28.f));
	Band->SetHorizontalAlignment(HAlign_Center);
	Band->SetVerticalAlignment(VAlign_Center);
	Band->SetContent(BossHUD::MakeText(WidgetTree, TEXT("PROJECT SUBMITTED"), 64, FLinearColor(0.85f, 0.68f, 0.3f), 200));

	UCanvasPanelSlot* BandSlot = Root->AddChildToCanvas(Band);
	BandSlot->SetAnchors(FAnchors(0.f, 0.5f, 1.f, 0.5f));
	BandSlot->SetAlignment(FVector2D(0.f, 0.5f));
	BandSlot->SetOffsets(FMargin(0.f));
	BandSlot->SetAutoSize(true);
	Victory = Band;
	Victory->SetRenderOpacity(0.f);
}

void UBossHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!Boss.IsValid())
	{
		TActorIterator<AVulcanBoss> It(GetWorld());
		Boss = It ? *It : nullptr;
	}
	const AVulcanBoss* Vulcan = Boss.Get();
	const UHealthComponent* Health = Vulcan ? Vulcan->FindComponentByClass<UHealthComponent>() : nullptr;
	if (!Health || !BossBar)
	{
		return;
	}

	// The trail holds where the health was and drains down to it.
	const float Percent = Health->GetHealthPercent();
	Fill->SetPercent(Percent);
	Trail->SetPercent(Percent > Trail->GetPercent() ? Percent : FMath::FInterpConstantTo(Trail->GetPercent(), Percent, InDeltaTime, 0.3f));

	if (Vulcan->IsDefeated())
	{
		// Bar fades out, then the banner fades in once the death animation has played a bit.
		DefeatedTime += InDeltaTime;
		BossBar->SetRenderOpacity(FMath::Clamp(1.f - DefeatedTime, 0.f, 1.f));
		Victory->SetRenderOpacity(FMath::Clamp((DefeatedTime - 1.5f) / 1.5f, 0.f, 1.f));
		return;
	}

	const float Shown = Vulcan->IsStatue() ? 0.f : 1.f;
	BossBar->SetRenderOpacity(FMath::FInterpConstantTo(BossBar->GetRenderOpacity(), Shown, InDeltaTime, 1.5f));
}

void UBossHUDSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.IsGameWorld())
	{
		// The player controller exists a moment after begin play.
		InWorld.GetTimerManager().SetTimer(SetupTimer, this, &UBossHUDSubsystem::AddToScreen, 0.2f, false);
	}
}

void UBossHUDSubsystem::AddToScreen()
{
	if (APlayerController* Controller = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (UBossHUDWidget* Widget = CreateWidget<UBossHUDWidget>(Controller, UBossHUDWidget::StaticClass()))
		{
			Widget->AddToViewport(5);
		}
	}
}
