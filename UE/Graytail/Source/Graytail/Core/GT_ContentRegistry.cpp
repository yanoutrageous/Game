#include "Core/GT_ContentRegistry.h"

namespace
{
	const FName GTRoomContent_EventDebugChoice01(TEXT("Event_DebugChoice_01"));
	const FName GTRoomRule_EventPresentOnly(TEXT("Event_PresentOnly"));
	const FName GTRoomContent_CombatDebugDummy01(TEXT("Combat_DebugDummy_01"));
	const FName GTRoomRule_CombatStartOnly(TEXT("Combat_StartOnly"));
	const FName GTEventOption_DefaultContinue(TEXT("Event_DebugOption_Continue"));
	const FName GTCombatResult_Success(TEXT("Success"));

	FGT_RoomContentDef MakeRoomContentDef(
		FName ContentId,
		FName DefaultRuleId,
		EGT_RoomBaseType RoomBaseType,
		const TCHAR* DisplayName,
		const TCHAR* DebugDescription,
		FName DefaultOptionId,
		FName DefaultResultId)
	{
		FGT_RoomContentDef Definition;
		Definition.ContentId = ContentId;
		Definition.DefaultRuleId = DefaultRuleId;
		Definition.RoomBaseType = RoomBaseType;
		Definition.DisplayName = DisplayName;
		Definition.DebugDescription = DebugDescription;
		Definition.DefaultOptionId = DefaultOptionId;
		Definition.DefaultResultId = DefaultResultId;
		return Definition;
	}

	FGT_RoomRuleDef MakeRoomRuleDef(
		FName RuleId,
		EGT_RoomBaseType RoomBaseType,
		const TCHAR* DisplayName,
		const TCHAR* DebugDescription,
		FName DefaultOptionId,
		FName DefaultResultId)
	{
		FGT_RoomRuleDef Definition;
		Definition.RuleId = RuleId;
		Definition.RoomBaseType = RoomBaseType;
		Definition.DisplayName = DisplayName;
		Definition.DebugDescription = DebugDescription;
		Definition.DefaultOptionId = DefaultOptionId;
		Definition.DefaultResultId = DefaultResultId;
		return Definition;
	}
}

void UGT_ContentRegistry::InitializeDefaultRoomDefinitions()
{
	RegisterRoomContentDef(MakeRoomContentDef(
		GTRoomContent_EventDebugChoice01,
		GTRoomRule_EventPresentOnly,
		EGT_RoomBaseType::Event,
		TEXT("Debug Event Choice"),
		TEXT("Prototype placeholder event content that can be presented and resolved through console play."),
		GTEventOption_DefaultContinue,
		NAME_None));

	RegisterRoomRuleDef(MakeRoomRuleDef(
		GTRoomRule_EventPresentOnly,
		EGT_RoomBaseType::Event,
		TEXT("Present Only Event"),
		TEXT("Prototype event rule that only presents a default continue option and resolves without rewards."),
		GTEventOption_DefaultContinue,
		NAME_None));

	RegisterRoomContentDef(MakeRoomContentDef(
		GTRoomContent_CombatDebugDummy01,
		GTRoomRule_CombatStartOnly,
		EGT_RoomBaseType::Combat,
		TEXT("Debug Dummy Combat"),
		TEXT("Prototype placeholder combat content that can be started and resolved through console play."),
		NAME_None,
		GTCombatResult_Success));

	RegisterRoomRuleDef(MakeRoomRuleDef(
		GTRoomRule_CombatStartOnly,
		EGT_RoomBaseType::Combat,
		TEXT("Start Only Combat"),
		TEXT("Prototype combat rule that starts placeholder combat and resolves only the Success result."),
		NAME_None,
		GTCombatResult_Success));
}

void UGT_ContentRegistry::RegisterContentId(FName ContentId)
{
	if (!ContentId.IsNone())
	{
		RegisteredContentIds.Add(ContentId);
	}
}

bool UGT_ContentRegistry::RegisterRoomContentDef(const FGT_RoomContentDef& Definition)
{
	if (Definition.ContentId.IsNone())
	{
		return false;
	}

	RegisteredContentIds.Add(Definition.ContentId);
	RoomContentDefs.Add(Definition.ContentId, Definition);
	return true;
}

bool UGT_ContentRegistry::RegisterRoomRuleDef(const FGT_RoomRuleDef& Definition)
{
	if (Definition.RuleId.IsNone())
	{
		return false;
	}

	RoomRuleDefs.Add(Definition.RuleId, Definition);
	return true;
}

bool UGT_ContentRegistry::FindRoomContentDef(FName ContentId, FGT_RoomContentDef& OutDefinition) const
{
	const FGT_RoomContentDef* Definition = RoomContentDefs.Find(ContentId);
	if (!Definition)
	{
		OutDefinition = FGT_RoomContentDef();
		return false;
	}

	OutDefinition = *Definition;
	return true;
}

bool UGT_ContentRegistry::FindRoomRuleDef(FName RuleId, FGT_RoomRuleDef& OutDefinition) const
{
	const FGT_RoomRuleDef* Definition = RoomRuleDefs.Find(RuleId);
	if (!Definition)
	{
		OutDefinition = FGT_RoomRuleDef();
		return false;
	}

	OutDefinition = *Definition;
	return true;
}

bool UGT_ContentRegistry::FindRoomDefinitions(
	FName ContentId,
	FName RuleId,
	EGT_RoomBaseType ExpectedRoomBaseType,
	FGT_RoomContentDef& OutContentDefinition,
	FGT_RoomRuleDef& OutRuleDefinition,
	FString& OutFailureReason) const
{
	OutContentDefinition = FGT_RoomContentDef();
	OutRuleDefinition = FGT_RoomRuleDef();
	OutFailureReason.Reset();

	if (!FindRoomContentDef(ContentId, OutContentDefinition))
	{
		OutFailureReason = FString::Printf(TEXT("RoomContentId=%s is not registered."), *ContentId.ToString());
		return false;
	}

	if (!FindRoomRuleDef(RuleId, OutRuleDefinition))
	{
		OutFailureReason = FString::Printf(TEXT("RoomRuleId=%s is not registered."), *RuleId.ToString());
		return false;
	}

	if (OutContentDefinition.RoomBaseType != ExpectedRoomBaseType)
	{
		OutFailureReason = FString::Printf(
			TEXT("RoomContentId=%s has RoomBaseType=%d but expected %d."),
			*ContentId.ToString(),
			static_cast<int32>(OutContentDefinition.RoomBaseType),
			static_cast<int32>(ExpectedRoomBaseType));
		return false;
	}

	if (OutRuleDefinition.RoomBaseType != ExpectedRoomBaseType)
	{
		OutFailureReason = FString::Printf(
			TEXT("RoomRuleId=%s has RoomBaseType=%d but expected %d."),
			*RuleId.ToString(),
			static_cast<int32>(OutRuleDefinition.RoomBaseType),
			static_cast<int32>(ExpectedRoomBaseType));
		return false;
	}

	if (!OutContentDefinition.DefaultRuleId.IsNone() && OutContentDefinition.DefaultRuleId != RuleId)
	{
		OutFailureReason = FString::Printf(
			TEXT("RoomContentId=%s default rule is %s but room has RuleId=%s."),
			*ContentId.ToString(),
			*OutContentDefinition.DefaultRuleId.ToString(),
			*RuleId.ToString());
		return false;
	}

	return true;
}

bool UGT_ContentRegistry::IsContentRegistered(FName ContentId) const
{
	return RegisteredContentIds.Contains(ContentId) || RoomContentDefs.Contains(ContentId);
}

void UGT_ContentRegistry::ClearRegistry()
{
	RegisteredContentIds.Reset();
	RoomContentDefs.Reset();
	RoomRuleDefs.Reset();
}
