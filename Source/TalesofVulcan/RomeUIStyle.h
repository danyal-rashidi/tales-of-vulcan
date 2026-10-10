#pragma once

#include "CoreMinimal.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"

/** The game's on-screen look, shared by the HUDs: Cinzel (/Game/Menu/Fonts) and the bronze 9-slice frame (/Game/UI). */
namespace RomeUIStyle
{
	inline FSlateFontInfo Cinzel(int32 Size, bool bBold = false, int32 LetterSpacing = 0)
	{
		static TSharedPtr<FCompositeFont> Fonts[2];
		const int32 Index = bBold ? 1 : 0;
		if (!Fonts[Index].IsValid())
		{
			if (UFontFace* Face = LoadObject<UFontFace>(nullptr, bBold ? TEXT("/Game/Menu/Fonts/Cinzel-Bold.Cinzel-Bold") : TEXT("/Game/Menu/Fonts/Cinzel-Regular.Cinzel-Regular"), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				Face->AddToRoot(); // kept for the life of the game, like the font it backs
				Fonts[Index] = MakeShared<FCompositeFont>();
				FTypefaceEntry& Entry = Fonts[Index]->DefaultTypeface.Fonts.AddDefaulted_GetRef();
				Entry.Name = TEXT("Regular");
				Entry.Font = FFontData(Face);
			}
		}
		FSlateFontInfo Info = Fonts[Index].IsValid() ? FSlateFontInfo(Fonts[Index], Size) : FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
		Info.LetterSpacing = LetterSpacing;
		return Info;
	}

	inline UTexture2D* Texture(const TCHAR* Name)
	{
		return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/UI/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** The thin bronze frame, stretched as a 9-slice box around whatever it borders. */
	inline FSlateBrush Frame()
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture(TEXT("T_UI_Frame")));
		Brush.ImageSize = FVector2D(64.f, 64.f);
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = FMargin(6.f / 64.f);
		return Brush;
	}
}
