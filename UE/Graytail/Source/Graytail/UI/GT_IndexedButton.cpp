#include "UI/GT_IndexedButton.h"

UGT_IndexedButton::UGT_IndexedButton()
{
	OnClicked.AddDynamic(this, &UGT_IndexedButton::HandleClicked);
}

void UGT_IndexedButton::HandleClicked()
{
	OnIndexClicked.Broadcast(Index);
}
