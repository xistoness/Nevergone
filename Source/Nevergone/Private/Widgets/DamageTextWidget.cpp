// Copyright Xyzto Works


#include "Widgets/DamageTextWidget.h"

#include "Components/TextBlock.h"

void UDamageTextWidget::SetDamage(float Damage)
{
	if (!DamageText) return;
	
	UE_LOG(LogTemp, Warning, TEXT("[DamageTextWidget]: Receiving damage: %f"), Damage);

	DamageText->SetText(FText::AsNumber(FMath::RoundToInt(Damage)));
}
