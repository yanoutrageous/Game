#pragma once

#include "CoreMinimal.h"

enum class EGT_EventKind : uint8;

// 单条教学弹窗定义(对齐 Lua GameText.tutorial.popupDefs)。纯 C++ 数据, 不进反射。
//   bBlocking            = 阻塞式: 抢焦点、锁移动, 必须确认才继续(开场/撤离说明)。
//   bOnce                = 只展示一次(确认后记入已展示集合, 不再弹)。
//   bRoomScoped          = 离开该房间时自动消失(各 rule 提示)。
//   bShowAfterRoomEffect = 进房效果(踩雷扣血)结算后再弹(UE 里移动是原子的, 进房回调时效果已结算)。
struct FGT_TutorialPopup
{
	FName Id;
	FString Title;
	FString Body;
	bool bBlocking = false;
	bool bOnce = false;
	bool bRoomScoped = true;
	bool bShowAfterRoomEffect = false;
	FString ConfirmText;
};

// 新手教程的坐标→弹窗映射 + 弹窗文案(对齐 Lua systems/Tutorial.lua 的 roomPopups +
// systems/GameText.lua 的 popupDefs)。坐标为 UE 0-based(Lua 1-based - 1)。
namespace GT_TutorialContent
{
	// 按教程格坐标返回该格的教学弹窗; 无映射返回 nullptr。
	GRAYTAIL_API const FGT_TutorialPopup* FindPopupForCell(int32 X, int32 Y);

	// 按弹窗 Id 返回定义(确认 once 弹窗时用); 无则 nullptr。
	GRAYTAIL_API const FGT_TutorialPopup* FindPopupById(FName Id);

	// 事件格教学弹窗按真实事件类型选取(旅商/赌徒/祭坛/机关), 让描述贴合当前房间。
	// 坐标映射里事件格统一指向占位 event_rule(=旅商), HUD 查到真实 Kind 后用本函数替换。
	GRAYTAIL_API const FGT_TutorialPopup* FindEventPopupForKind(EGT_EventKind Kind);
}
