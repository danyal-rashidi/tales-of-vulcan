#include "GameHUDWidget.h"
#include "HealthComponent.h"
#include "PlayerEquipComponent.h"
#include "PlayerHUDWidget.h"
#include "RomeUIStyle.h"
#include "StaminaComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Landscape.h"
#include "UObject/UObjectIterator.h"

namespace GameHUD
{
	const FLinearColor Parchment(0.92f, 0.86f, 0.74f);
	const FLinearColor Muted(0.62f, 0.58f, 0.5f);
	const FLinearColor Bronze(0.78f, 0.6f, 0.32f);

	UProgressBar* Bar(UWidgetTree* Tree, const FLinearColor& Back, const FLinearColor& Front)
	{
		UProgressBar* B = Tree->ConstructWidget<UProgressBar>();
		FProgressBarStyle Style = B->GetWidgetStyle();
		Style.SetBackgroundImage(FSlateColorBrush(Back));
		Style.SetFillImage(FSlateColorBrush(FLinearColor::White));
		B->SetWidgetStyle(Style);
		B->SetFillColorAndOpacity(Front);
		B->SetPercent(1.f);
		return B;
	}

	UTextBlock* Text(UWidgetTree* Tree, const FString& String, int32 Size, const FLinearColor& Color, int32 Spacing = 0, bool bBold = false)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
		T->SetText(FText::FromString(String));
		T->SetFont(RomeUIStyle::Cinzel(Size, bBold, Spacing));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetShadowOffset(FVector2D(1.5f, 2.f));
		T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		return T;
	}

	/** A bar in the bronze frame: fill over trail over a dark back. */
	UWidget* FramedBar(UWidgetTree* Tree, float Width, float Height, UProgressBar* Back, UProgressBar* Front)
	{
		USizeBox* Size = Tree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(Width);
		Size->SetHeightOverride(Height);
		UOverlay* Layers = Tree->ConstructWidget<UOverlay>();
		for (UProgressBar* B : { Back, Front })
		{
			if (B)
			{
				UOverlaySlot* S = Layers->AddChildToOverlay(B);
				S->SetHorizontalAlignment(HAlign_Fill);
				S->SetVerticalAlignment(VAlign_Fill);
			}
		}
		Size->SetContent(Layers);
		UBorder* Frame = Tree->ConstructWidget<UBorder>();
		Frame->SetBrush(RomeUIStyle::Frame());
		Frame->SetPadding(FMargin(5.f));
		Frame->SetContent(Size);
		return Frame;
	}

	/** A thin bronze rule, for the area banner. */
	UWidget* Rule(UWidgetTree* Tree, float Width)
	{
		USizeBox* Size = Tree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(Width);
		Size->SetHeightOverride(1.5f);
		UBorder* Line = Tree->ConstructWidget<UBorder>();
		Line->SetBrushColor(Bronze * FLinearColor(1.f, 1.f, 1.f, 0.85f));
		Size->SetContent(Line);
		return Size;
	}
}

void UGameHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	using namespace GameHUD;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;
	auto Place = [Root](UWidget* W, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(W);
		S->SetAnchors(Anchors);
		S->SetAlignment(Alignment);
		S->SetPosition(Position);
		S->SetAutoSize(true);
		return S;
	};

	// ---- the red edge (low health, hits), under everything
	Vignette = WidgetTree->ConstructWidget<UImage>();
	Vignette->SetBrushFromTexture(RomeUIStyle::Texture(TEXT("T_UI_Vignette")));
	Vignette->SetRenderOpacity(0.f);
	UCanvasPanelSlot* VSlot = Root->AddChildToCanvas(Vignette);
	VSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	VSlot->SetOffsets(FMargin(0.f));

	// ---- health and stamina, top left
	UVerticalBox* Bars = WidgetTree->ConstructWidget<UVerticalBox>();
	HealthTrail = Bar(WidgetTree, FLinearColor(0.015f, 0.01f, 0.01f, 0.8f), FLinearColor(0.82f, 0.62f, 0.3f));
	HealthFill = Bar(WidgetTree, FLinearColor::Transparent, FLinearColor(0.52f, 0.035f, 0.025f));
	Bars->AddChildToVerticalBox(FramedBar(WidgetTree, 380.f, 12.f, HealthTrail, HealthFill))->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	StaminaFill = Bar(WidgetTree, FLinearColor(0.015f, 0.01f, 0.01f, 0.8f), FLinearColor(0.32f, 0.42f, 0.16f));
	Bars->AddChildToVerticalBox(FramedBar(WidgetTree, 300.f, 8.f, nullptr, StaminaFill));
	Place(Bars, FAnchors(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(42.f, 36.f));

	// ---- the weapon slot, bottom left
	UHorizontalBox* Weapon = WidgetTree->ConstructWidget<UHorizontalBox>();
	USizeBox* SlotSize = WidgetTree->ConstructWidget<USizeBox>();
	SlotSize->SetWidthOverride(92.f);
	SlotSize->SetHeightOverride(92.f);
	UBorder* SlotFrame = WidgetTree->ConstructWidget<UBorder>();
	SlotFrame->SetBrush(RomeUIStyle::Frame());
	SlotFrame->SetPadding(FMargin(5.f));
	UBorder* SlotBack = WidgetTree->ConstructWidget<UBorder>();
	SlotBack->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	SlotBack->SetPadding(FMargin(6.f));
	SpearIcon = WidgetTree->ConstructWidget<UImage>();
	SpearIcon->SetBrushFromTexture(RomeUIStyle::Texture(TEXT("T_UI_Spear")));
	SlotBack->SetContent(SpearIcon);
	SlotFrame->SetContent(SlotBack);
	SlotSize->SetContent(SlotFrame);
	Weapon->AddChildToHorizontalBox(SlotSize);
	UVerticalBox* WeaponText = WidgetTree->ConstructWidget<UVerticalBox>();
	WeaponText->AddChildToVerticalBox(Text(WidgetTree, TEXT("Spear"), 20, Parchment, 60));
	SpearState = Text(WidgetTree, TEXT("in hand"), 13, Muted, 40);
	WeaponText->AddChildToVerticalBox(SpearState);
	SpearHint = Text(WidgetTree, TEXT("R  sheathe"), 12, Bronze, 30);
	WeaponText->AddChildToVerticalBox(SpearHint)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	UHorizontalBoxSlot* TextSlot = Weapon->AddChildToHorizontalBox(WeaponText);
	TextSlot->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
	TextSlot->SetVerticalAlignment(VAlign_Center);
	Place(Weapon, FAnchors(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(42.f, -40.f));

	// ---- the area name
	UVerticalBox* Banner = WidgetTree->ConstructWidget<UVerticalBox>();
	Banner->AddChildToVerticalBox(Rule(WidgetTree, 520.f))->SetHorizontalAlignment(HAlign_Center);
	AreaText = Text(WidgetTree, TEXT(""), 38, Parchment, 140);
	UVerticalBoxSlot* AreaSlot = Banner->AddChildToVerticalBox(AreaText);
	AreaSlot->SetHorizontalAlignment(HAlign_Center);
	AreaSlot->SetPadding(FMargin(0.f, 10.f));
	Banner->AddChildToVerticalBox(Rule(WidgetTree, 520.f))->SetHorizontalAlignment(HAlign_Center);
	AreaBanner = Banner;
	AreaBanner->SetRenderOpacity(0.f);
	Place(Banner, FAnchors(0.5f, 0.7f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

	// ---- the controls, bottom right, for the first moments
	UVerticalBox* Help = WidgetTree->ConstructWidget<UVerticalBox>();
	for (const TCHAR* Line : { TEXT("WASD  move      Mouse  look"), TEXT("Left / Right mouse  light / heavy attack"),
		TEXT("Shift  tap to roll, hold to sprint"), TEXT("Space  jump      R  draw / sheathe spear") })
	{
		Help->AddChildToVerticalBox(Text(WidgetTree, Line, 12, Muted, 20))->SetHorizontalAlignment(HAlign_Right);
	}
	Controls = Help;
	Place(Help, FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-42.f, -40.f));

	// ---- YOU DIED: dark red letters on a dark band across the middle
	UBorder* Band = WidgetTree->ConstructWidget<UBorder>();
	Band->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.72f));
	Band->SetPadding(FMargin(0.f, 34.f));
	Band->SetHorizontalAlignment(HAlign_Center);
	Band->SetVerticalAlignment(VAlign_Center);
	Band->SetContent(Text(WidgetTree, TEXT("YOU DIED"), 78, FLinearColor(0.5f, 0.03f, 0.02f), 220));
	UCanvasPanelSlot* BandSlot = Root->AddChildToCanvas(Band);
	BandSlot->SetAnchors(FAnchors(0.f, 0.5f, 1.f, 0.5f));
	BandSlot->SetAlignment(FVector2D(0.f, 0.5f));
	BandSlot->SetOffsets(FMargin(0.f));
	BandSlot->SetAutoSize(true);
	DiedBand = Band;
	DiedBand->SetRenderOpacity(0.f);

	bOpenMap = TActorIterator<ALandscapeProxy>(GetWorld()) ? true : false;
}

FString UGameHUDWidget::AreaAt(const FVector& Location) const
{
	// Metres from the colosseum's centre (the plan's rterrain.py layout; +Y is north).
	const FVector2D P = (FVector2D(Location) - FVector2D(-44.f, -5.f)) / 100.f;
	auto InEllipse = [&P](const FVector2D& C, float Rx, float Ry) { return FMath::Square((P.X - C.X) / Rx) + FMath::Square((P.Y - C.Y) / Ry) <= 1.f; };
	if (InEllipse(FVector2D::ZeroVector, 56.f, 46.f))
	{
		return TEXT("The Colosseum");
	}
	if (P.X > -53.f && P.X < 53.f && P.Y > 277.f && P.Y < 383.f)
	{
		return TEXT("The Forum");
	}
	if (InEllipse(FVector2D(-390.f, 430.f), 90.f, 80.f))
	{
		return TEXT("The Temple Hill");
	}
	if (InEllipse(FVector2D(560.f, 170.f), 150.f, 115.f))
	{
		return TEXT("The Oasis");
	}
	if (P.X > -270.f && P.X < 270.f && P.Y > 90.f && P.Y < 570.f)
	{
		return TEXT("The Town");
	}
	return TEXT("The Desert");
}

void UGameHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;

	// The old Blueprint HUD is created by the character a moment after play starts: keep it hidden.
	if (Time < 6.f)
	{
		for (TObjectIterator<UPlayerHUDWidget> It; It; ++It)
		{
			if (It->GetWorld() == GetWorld() && It->GetVisibility() != ESlateVisibility::Collapsed)
			{
				It->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}

	const APawn* Pawn = GetOwningPlayerPawn();
	if (!Pawn)
	{
		return;
	}

	// ---- bars
	if (const UHealthComponent* Health = Pawn->FindComponentByClass<UHealthComponent>())
	{
		const float Percent = Health->GetHealthPercent();
		if (LastHealth >= 0.f && Percent < LastHealth - 0.001f)
		{
			HitFlash = 1.f;
		}
		LastHealth = Percent;
		HealthFill->SetPercent(Percent);
		HealthTrail->SetPercent(Percent > HealthTrail->GetPercent() ? Percent : FMath::FInterpConstantTo(HealthTrail->GetPercent(), Percent, InDeltaTime, 0.35f));

		// Red edge: a heartbeat below a third of health, a flash on every hit.
		HitFlash = FMath::Max(0.f, HitFlash - InDeltaTime * 1.6f);
		const float Low = FMath::Clamp((0.33f - Percent) / 0.33f, 0.f, 1.f);
		const float Beat = Low * (0.55f + 0.35f * FMath::Pow(FMath::Abs(FMath::Sin(Time * 2.6f)), 6.f));
		Vignette->SetRenderOpacity(FMath::Clamp(FMath::Max(Beat, HitFlash * 0.85f), 0.f, 1.f));

		if (Health->IsDead())
		{
			DeadTime = DeadTime < 0.f ? 0.f : DeadTime + InDeltaTime;
			DiedBand->SetRenderOpacity(FMath::Clamp((DeadTime - 0.8f) / 2.f, 0.f, 1.f));
		}
	}
	if (const UStaminaComponent* Stamina = Pawn->FindComponentByClass<UStaminaComponent>())
	{
		const float Percent = Stamina->GetStaminaPercent();
		StaminaFill->SetPercent(Percent);
		// Nearly spent: the bar pales and pulses.
		const float Empty = Percent < 0.12f ? 0.5f + 0.5f * FMath::Sin(Time * 12.f) : 0.f;
		StaminaFill->SetFillColorAndOpacity(FMath::Lerp(FLinearColor(0.32f, 0.42f, 0.16f), FLinearColor(0.75f, 0.7f, 0.45f), Empty));
	}

	// ---- weapon slot
	if (const UPlayerEquipComponent* Equip = Pawn->FindComponentByClass<UPlayerEquipComponent>())
	{
		const bool bArmed = Equip->IsArmed();
		SpearIcon->SetColorAndOpacity(bArmed ? FLinearColor::White : FLinearColor(0.45f, 0.42f, 0.38f, 0.7f));
		SpearState->SetText(FText::FromString(Equip->IsBusy() ? TEXT("...") : (bArmed ? TEXT("in hand") : TEXT("on your back"))));
		SpearHint->SetText(FText::FromString(bArmed ? TEXT("R  sheathe") : TEXT("R  draw")));
	}

	// ---- controls fade after a while
	Controls->SetRenderOpacity(FMath::Clamp((22.f - Time) / 3.f, 0.f, 1.f) * FMath::Clamp(Time / 1.5f, 0.f, 1.f));

	// ---- area names (the open map only): a new one shows once the player has been in it for a moment
	if (bOpenMap)
	{
		const FString Here = AreaAt(Pawn->GetActorLocation());
		if (Here != Area)
		{
			if (Here != PendingArea)
			{
				PendingArea = Here;
				PendingTime = 0.f;
			}
			PendingTime += InDeltaTime;
			if (PendingTime > (Area.IsEmpty() ? 1.2f : 1.5f))
			{
				Area = Here;
				AreaText->SetText(FText::FromString(Area));
				BannerTime = 0.f;
			}
		}
		if (BannerTime >= 0.f)
		{
			BannerTime += InDeltaTime;
			const float In = FMath::Clamp(BannerTime / 1.2f, 0.f, 1.f);
			const float Out = FMath::Clamp((5.2f - BannerTime) / 1.5f, 0.f, 1.f);
			AreaBanner->SetRenderOpacity(FMath::Min(In, Out));
			if (BannerTime > 5.2f)
			{
				BannerTime = -1.f;
			}
		}
	}
}
