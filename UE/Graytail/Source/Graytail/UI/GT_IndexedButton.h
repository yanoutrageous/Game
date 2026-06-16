#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "GT_IndexedButton.generated.h"

// 带 Index 的按钮: 列表里每行一个, 点击时把自己的 Index 广播出去,
// 解决 UButton::OnClicked 无参数、无法区分是哪一行的问题(纯 C++ UMG 列表常用模式)。
UCLASS()
class GRAYTAIL_API UGT_IndexedButton : public UButton
{
	GENERATED_BODY()

public:
	UGT_IndexedButton();

	UPROPERTY(Transient)
	int32 Index = 0;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGT_IndexButtonClicked, int32, Index);

	UPROPERTY()
	FGT_IndexButtonClicked OnIndexClicked;

private:
	UFUNCTION()
	void HandleClicked();
};
