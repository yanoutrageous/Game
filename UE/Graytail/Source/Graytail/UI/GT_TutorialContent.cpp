#include "UI/GT_TutorialContent.h"

#include "Domains/Events/GT_EventTypes.h"

namespace
{
	// 弹窗 Id 常量(与 Lua popupDefs 键一致; 后三个事件变体为 UE 新增, 对齐 EGT_EventKind)。
	const FName GTTutorial_SpawnIntro(TEXT("spawn_intro"));
	const FName GTTutorial_NumberRule(TEXT("number_rule"));
	const FName GTTutorial_MineRule(TEXT("mine_rule"));
	const FName GTTutorial_EventRule(TEXT("event_rule"));    // 旅商(Trader)
	const FName GTTutorial_DiceRule(TEXT("dice_rule"));      // 赌徒(Dice)
	const FName GTTutorial_AltarRule(TEXT("altar_rule"));    // 祭坛(Altar)
	const FName GTTutorial_TrapRule(TEXT("trap_rule"));      // 机关(Trap)
	const FName GTTutorial_MonsterRule(TEXT("monster_rule"));
	const FName GTTutorial_ChestRule(TEXT("chest_rule"));
	const FName GTTutorial_MapRule(TEXT("map_rule"));
	const FName GTTutorial_MineReview(TEXT("mine_review"));
	const FName GTTutorial_RouteRule(TEXT("route_rule"));
	const FName GTTutorial_ExitGoal(TEXT("exit_goal"));

	// 全部弹窗定义(对齐 GameText.lua popupDefs, 正文逐行照搬)。首次访问时惰性构建。
	const TArray<FGT_TutorialPopup>& GetPopupDefs()
	{
		static const TArray<FGT_TutorialPopup> Defs = []()
		{
			TArray<FGT_TutorialPopup> Out;

			{
				FGT_TutorialPopup P;
				P.Id = GTTutorial_SpawnIntro;
				P.Title = TEXT("新员工说明");
				P.Body = TEXT("欢迎入职灰尾回收。\n\n你是封锁区临时回收员。\n本次目标是读取区域扫描图，\n避开雷险，搜刮物资，\n并找到撤离信标。\n\n调度台 A-7：\n公司建议你带回物资。\n更建议你带回自己。");
				P.bBlocking = true;
				P.bOnce = true;
				P.bRoomScoped = false;
				P.ConfirmText = TEXT("开始作业");
				Out.Add(P);
			}
			{
				FGT_TutorialPopup P;
				P.Id = GTTutorial_NumberRule;
				P.Title = TEXT("区域扫描图");
				P.Body = TEXT("房间数字表示周围 8 个区域中的\n雷险数量。\n\n上下左右和斜向都会计入数字。\n异常体、物资、事件和撤离信标\n不计入该数字。\n\n调度台 A-7：\n数字通常不会骗人。\n公司系统另行计算。");
				Out.Add(P);
			}
			{
				FGT_TutorialPopup P;
				P.Id = GTTutorial_MineRule;
				P.Title = TEXT("雷险区");
				P.Body = TEXT("雷险区会造成伤害，\n并提高封锁压力。\n\n封锁压力升高时，\n五四三二一撤离协议可能下降。\n\n已触发的雷险会被记录，\n再次经过不会重复触发。\n\n调度台 A-7：\n协议下降不是惩罚。\n只是公司提前声明提醒过你。");
				P.bShowAfterRoomEffect = true;
				Out.Add(P);
			}
			{
				FGT_TutorialPopup P;
				P.Id = GTTutorial_EventRule;
				P.Title = TEXT("狐狸旅商");
				P.Body = TEXT("旅商可以将异常回收物\n折价出售为已锁定收益。\n\n已锁定收益即使作业失败\n也会保留。\n\n调度台 A-7：\n公司不会知道，大概。");
				Out.Add(P);
			}
			{
				FGT_TutorialPopup P;
				P.Id = GTTutorial_DiceRule;
				P.Title = TEXT("赌徒");
				P.Body = TEXT("赌徒会用一局骰子\n赌你的待结算收益。\n\n押注赢了能翻倍入账，\n输了则折损一部分。\n\n调度台 A-7：\n公司不参与赌博。\n公司只统计结果。");
				Out.Add(P);
			}
			{
				FGT_TutorialPopup P;
				P.Id = GTTutorial_AltarRule;
				P.Title = TEXT("异常祭坛");
				P.Body = TEXT("祭坛可以献祭生命值，\n换取已锁定收益。\n\n献祭不会让你当场倒下，\n但每献一次都更贵。\n\n调度台 A-7：\n虔诚也是一种成本。");
				Out.Add(P);
			}
			{
				FGT_TutorialPopup P;
				P.Id = GTTutorial_TrapRule;
				P.Title = TEXT("机关装置");
				P.Body = TEXT("机关需要你动手拆解。\n\n战斗力足够时拆解成功，\n能拿到奖励; 失败会受伤并升压。\n\n调度台 A-7：\n拆解手册第一页写着：\n先确认值不值。");
				Out.Add(P);
			}
			{
				FGT_TutorialPopup P;
				P.Id = GTTutorial_MonsterRule;
				P.Title = TEXT("异常体区域");
				P.Body = TEXT("异常体区域可以绕行，\n也可以清理。\n\n靠近后按 F 攻击。\n清理异常体会获得奖励，\n但也会提高封锁压力。\n\n调度台 A-7：\n高收益区和高事故区\n通常是同一个地方。");
				Out.Add(P);
			}
			{
				FGT_TutorialPopup P;
				P.Id = GTTutorial_ChestRule;
				P.Title = TEXT("物资箱");
				P.Body = TEXT("发现物资箱时，按 F 开启。\n\n物资箱通常比普通搜索\n更有价值，\n但收益仍需成功撤离后结算。\n\n调度台 A-7：\n箱子归你，风险也归你。");
				Out.Add(P);
			}
			{
				FGT_TutorialPopup P;
				P.Id = GTTutorial_MapRule;
				P.Title = TEXT("区域扫描图操作");
				P.Body = TEXT("按 M 打开区域扫描图。\n\n你可以查看已探索区域，\n也可以标记怀疑存在雷险的位置。\n\n点击已探索区域，\n可以回传到对应房间。\n\n调度台 A-7：\n回头不是失败。失联才是。");
				Out.Add(P);
			}
			{
				FGT_TutorialPopup P;
				P.Id = GTTutorial_MineReview;
				P.Title = TEXT("雷险复查");
				P.Body = TEXT("再次遇到雷险时，\n先观察周围数字。\n\n如果路线风险过高，\n可以打开区域扫描图重新规划，\n或回传到已探索区域。\n\n调度台 A-7：\n第二次踩中同类风险时，\n系统会将其归类为经验不足。");
				P.bShowAfterRoomEffect = true;
				Out.Add(P);
			}
			{
				FGT_TutorialPopup P;
				P.Id = GTTutorial_RouteRule;
				P.Title = TEXT("路线规划");
				P.Body = TEXT("区域扫描图可以帮助你\n重新规划路线。\n\n已探索区域可以点击回传，\n适合在协议下降后调整路径。\n\n调度台 A-7：\n合理返程不影响绩效。\n失联会。");
				Out.Add(P);
			}
			{
				FGT_TutorialPopup P;
				P.Id = GTTutorial_ExitGoal;
				P.Title = TEXT("撤离信标");
				P.Body = TEXT("到达撤离信标后，\n按 E 打开撤离确认。\n\n成功撤离会结算待结算收益，\n并带回回收包中的异常回收物。\n\n五四三二一撤离协议\n表示当前封锁区风险。\n协议数字越低，\n调度台越不建议继续深入。\n\n调度台 A-7：\n撤离是建议。\n不撤离是自主选择。\n相关条款已说明。");
				P.bBlocking = true;
				P.bOnce = true;
				P.bRoomScoped = false;
				P.ConfirmText = TEXT("我知道了");
				Out.Add(P);
			}

			return Out;
		}();
		return Defs;
	}

	// 坐标→弹窗 Id 映射(对齐 Tutorial.lua roomPopups, 1-based→0-based)。
	// 反对角线带状: 出生→数字→雷→事件→怪物→宝箱, 加 map/mine_review/route/exit 散点。覆盖全部 25 格。
	struct FGTTutorialCellMap { int32 X; int32 Y; const FName* Id; };
	const TArray<FGTTutorialCellMap>& GetCellMap()
	{
		static const TArray<FGTTutorialCellMap> Map = {
			{ 0, 0, &GTTutorial_SpawnIntro },
			{ 0, 1, &GTTutorial_NumberRule }, { 1, 0, &GTTutorial_NumberRule },
			{ 0, 2, &GTTutorial_MineRule }, { 1, 1, &GTTutorial_MineRule }, { 2, 0, &GTTutorial_MineRule },
			{ 0, 3, &GTTutorial_EventRule }, { 1, 2, &GTTutorial_EventRule }, { 2, 1, &GTTutorial_EventRule }, { 3, 0, &GTTutorial_EventRule },
			{ 0, 4, &GTTutorial_MonsterRule }, { 1, 3, &GTTutorial_MonsterRule }, { 2, 2, &GTTutorial_MonsterRule }, { 3, 1, &GTTutorial_MonsterRule }, { 4, 0, &GTTutorial_MonsterRule },
			{ 1, 4, &GTTutorial_ChestRule }, { 2, 3, &GTTutorial_ChestRule }, { 3, 2, &GTTutorial_ChestRule }, { 4, 1, &GTTutorial_ChestRule },
			{ 2, 4, &GTTutorial_MapRule }, { 4, 2, &GTTutorial_MapRule },
			{ 3, 3, &GTTutorial_MineReview },
			{ 3, 4, &GTTutorial_RouteRule }, { 4, 3, &GTTutorial_RouteRule },
			{ 4, 4, &GTTutorial_ExitGoal },
		};
		return Map;
	}
}

namespace GT_TutorialContent
{
	const FGT_TutorialPopup* FindPopupForCell(int32 X, int32 Y)
	{
		for (const FGTTutorialCellMap& Entry : GetCellMap())
		{
			if (Entry.X == X && Entry.Y == Y)
			{
				return FindPopupById(*Entry.Id);
			}
		}
		return nullptr;
	}

	const FGT_TutorialPopup* FindPopupById(FName Id)
	{
		for (const FGT_TutorialPopup& Popup : GetPopupDefs())
		{
			if (Popup.Id == Id)
			{
				return &Popup;
			}
		}
		return nullptr;
	}

	const FGT_TutorialPopup* FindEventPopupForKind(EGT_EventKind Kind)
	{
		switch (Kind)
		{
		case EGT_EventKind::Dice:  return FindPopupById(GTTutorial_DiceRule);
		case EGT_EventKind::Altar: return FindPopupById(GTTutorial_AltarRule);
		case EGT_EventKind::Trap:  return FindPopupById(GTTutorial_TrapRule);
		case EGT_EventKind::Trader:
		default:                   return FindPopupById(GTTutorial_EventRule);
		}
	}
}
