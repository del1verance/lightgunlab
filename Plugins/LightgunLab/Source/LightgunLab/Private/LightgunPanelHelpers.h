// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Styling/CoreStyle.h"

// Shared by the startup and options panels; inline so unity builds don't collide.

inline UTextBlock* MakePanelText(UWidgetTree* Tree, const FString& Text, int32 FontSize, bool bBold, FLinearColor Color = FLinearColor::White)
{
	UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(FText::FromString(Text));
	Block->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FontSize));
	Block->SetColorAndOpacity(FSlateColor(Color));
	return Block;
}

inline UButton* MakePanelButton(UWidgetTree* Tree, const FString& Label)
{
	UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* Text = MakePanelText(Tree, Label, 12, true, FLinearColor(0.05f, 0.05f, 0.06f));
	Button->AddChild(Text);
	return Button;
}

inline USlider* MakePanelSlider(UWidgetTree* Tree, float Min, float Max, float Value)
{
	USlider* Slider = Tree->ConstructWidget<USlider>(USlider::StaticClass());
	Slider->SetMinValue(Min);
	Slider->SetMaxValue(Max);
	Slider->SetValue(Value);
	return Slider;
}
