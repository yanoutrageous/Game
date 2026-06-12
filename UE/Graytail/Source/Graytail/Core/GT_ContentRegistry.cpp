#include "Core/GT_ContentRegistry.h"

namespace
{
	const FName GTRoomContent_EventDebugChoice01(TEXT("Event_DebugChoice_01"));
	const FName GTRoomRule_EventPresentOnly(TEXT("Event_PresentOnly"));
	const FName GTRoomContent_CombatDebugDummy01(TEXT("Combat_DebugDummy_01"));
	const FName GTRoomRule_CombatStartOnly(TEXT("Combat_StartOnly"));
	const FName GTEventOption_DefaultContinue(TEXT("Event_DebugOption_Continue"));
	const FName GTEventOption_Scout(TEXT("Event_DebugOption_Scout"));
	const FName GTCombatResult_Success(TEXT("Combat_DebugResult_Success"));
	const FName GTCombatResult_Retreat(TEXT("Combat_DebugResult_Retreat"));
	const FName GTEventType_CombatResolved(TEXT("CombatResolved"));
	const FName GTEventType_CombatRetreated(TEXT("CombatRetreated"));

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

	FGT_EventOptionDef MakeEventOptionDef(
		FName OptionId,
		const TCHAR* DisplayName,
		const TCHAR* DebugDescription,
		bool bResolvesRoom,
		const TCHAR* PayloadText)
	{
		FGT_EventOptionDef Definition;
		Definition.Id = OptionId;
		Definition.DisplayName = DisplayName;
		Definition.DebugDescription = DebugDescription;
		Definition.bResolvesRoom = bResolvesRoom;
		Definition.PayloadText = PayloadText;
		return Definition;
	}

	FGT_CombatResultDef MakeCombatResultDef(
		FName ResultId,
		const TCHAR* DisplayName,
		const TCHAR* DebugDescription,
		bool bResolvesRoom,
		const TCHAR* PayloadText,
		FName EventType)
	{
		FGT_CombatResultDef Definition;
		Definition.Id = ResultId;
		Definition.DisplayName = DisplayName;
		Definition.DebugDescription = DebugDescription;
		Definition.bResolvesRoom = bResolvesRoom;
		Definition.PayloadText = PayloadText;
		Definition.EventType = EventType;
		return Definition;
	}

	void AddUniqueId(TArray<FName>& OutIds, FName Id)
	{
		if (!Id.IsNone())
		{
			OutIds.AddUnique(Id);
		}
	}

	void AddUniqueIds(TArray<FName>& OutIds, const TArray<FName>& Ids)
	{
		for (const FName Id : Ids)
		{
			AddUniqueId(OutIds, Id);
		}
	}

	void BuildAvailableEventOptionIds(
		const FGT_RoomContentDef& ContentDefinition,
		const FGT_RoomRuleDef& RuleDefinition,
		TArray<FName>& OutIds)
	{
		OutIds.Reset();
		AddUniqueIds(OutIds, ContentDefinition.AvailableOptionIds);
		AddUniqueIds(OutIds, RuleDefinition.AvailableOptionIds);
		AddUniqueId(OutIds, RuleDefinition.DefaultOptionId);
		AddUniqueId(OutIds, ContentDefinition.DefaultOptionId);
	}

	void BuildAvailableCombatResultIds(
		const FGT_RoomContentDef& ContentDefinition,
		const FGT_RoomRuleDef& RuleDefinition,
		TArray<FName>& OutIds)
	{
		OutIds.Reset();
		AddUniqueIds(OutIds, ContentDefinition.AvailableResultIds);
		AddUniqueIds(OutIds, RuleDefinition.AvailableResultIds);
		AddUniqueId(OutIds, RuleDefinition.DefaultResultId);
		AddUniqueId(OutIds, ContentDefinition.DefaultResultId);
	}

	FString BuildIdListText(const TArray<FName>& Ids)
	{
		TArray<FString> Parts;
		Parts.Reserve(Ids.Num());
		for (const FName Id : Ids)
		{
			Parts.Add(Id.ToString());
		}

		return FString::Join(Parts, TEXT(", "));
	}
}

void UGT_ContentRegistry::InitializeDefaultRoomDefinitions()
{
	FGT_RoomContentDef EventContentDefinition = MakeRoomContentDef(
		GTRoomContent_EventDebugChoice01,
		GTRoomRule_EventPresentOnly,
		EGT_RoomBaseType::Event,
		TEXT("Debug Event Choice"),
		TEXT("Prototype placeholder event content that can be presented and resolved through console play."),
		GTEventOption_DefaultContinue,
		NAME_None);
	EventContentDefinition.AvailableOptionIds.Add(GTEventOption_DefaultContinue);
	EventContentDefinition.AvailableOptionIds.Add(GTEventOption_Scout);
	RegisterRoomContentDef(EventContentDefinition);

	FGT_RoomRuleDef EventRuleDefinition = MakeRoomRuleDef(
		GTRoomRule_EventPresentOnly,
		EGT_RoomBaseType::Event,
		TEXT("Present Only Event"),
		TEXT("Prototype event rule that only presents a default continue option and resolves without rewards."),
		GTEventOption_DefaultContinue,
		NAME_None);
	EventRuleDefinition.AvailableOptionIds.Add(GTEventOption_DefaultContinue);
	EventRuleDefinition.AvailableOptionIds.Add(GTEventOption_Scout);
	RegisterRoomRuleDef(EventRuleDefinition);

	RegisterEventOptionDef(MakeEventOptionDef(
		GTEventOption_DefaultContinue,
		TEXT("Continue"),
		TEXT("Placeholder event option that simply resolves the current Event room."),
		true,
		TEXT("Continue option selected; Event room resolved without rewards.")));

	RegisterEventOptionDef(MakeEventOptionDef(
		GTEventOption_Scout,
		TEXT("Scout"),
		TEXT("Placeholder event option that records a Scout choice without revealing map data or granting rewards."),
		true,
		TEXT("Scout option selected; no scan reward, reveal, item, or resource effect is applied.")));

	FGT_RoomContentDef CombatContentDefinition = MakeRoomContentDef(
		GTRoomContent_CombatDebugDummy01,
		GTRoomRule_CombatStartOnly,
		EGT_RoomBaseType::Combat,
		TEXT("Debug Dummy Combat"),
		TEXT("Prototype placeholder combat content that can be started and resolved through console play."),
		NAME_None,
		GTCombatResult_Success);
	CombatContentDefinition.AvailableResultIds.Add(GTCombatResult_Success);
	CombatContentDefinition.AvailableResultIds.Add(GTCombatResult_Retreat);
	RegisterRoomContentDef(CombatContentDefinition);

	FGT_RoomRuleDef CombatRuleDefinition = MakeRoomRuleDef(
		GTRoomRule_CombatStartOnly,
		EGT_RoomBaseType::Combat,
		TEXT("Start Only Combat"),
		TEXT("Prototype combat rule that starts placeholder combat and resolves a data-defined placeholder result."),
		NAME_None,
		GTCombatResult_Success);
	CombatRuleDefinition.AvailableResultIds.Add(GTCombatResult_Success);
	CombatRuleDefinition.AvailableResultIds.Add(GTCombatResult_Retreat);
	RegisterRoomRuleDef(CombatRuleDefinition);

	RegisterCombatResultDef(MakeCombatResultDef(
		GTCombatResult_Success,
		TEXT("Success"),
		TEXT("Placeholder combat result that resolves the current Combat room without run failure, damage, loot, or rewards."),
		true,
		TEXT("Combat placeholder resolved as Success; no HP, damage, loot, reward, or RunState effect is applied."),
		GTEventType_CombatResolved));

	RegisterCombatResultDef(MakeCombatResultDef(
		GTCombatResult_Retreat,
		TEXT("Retreat"),
		TEXT("Placeholder combat result that records a retreat and resolves the current Combat room without failing the run."),
		true,
		TEXT("Combat placeholder resolved as Retreat; no RunFailed, HP, damage, loot, or reward effect is applied."),
		GTEventType_CombatRetreated));
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

bool UGT_ContentRegistry::RegisterEventOptionDef(const FGT_EventOptionDef& Definition)
{
	if (Definition.Id.IsNone())
	{
		return false;
	}

	EventOptionDefs.Add(Definition.Id, Definition);
	return true;
}

bool UGT_ContentRegistry::RegisterCombatResultDef(const FGT_CombatResultDef& Definition)
{
	if (Definition.Id.IsNone())
	{
		return false;
	}

	CombatResultDefs.Add(Definition.Id, Definition);
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

bool UGT_ContentRegistry::FindEventOptionDef(FName OptionId, FGT_EventOptionDef& OutDefinition) const
{
	const FGT_EventOptionDef* Definition = EventOptionDefs.Find(OptionId);
	if (!Definition)
	{
		OutDefinition = FGT_EventOptionDef();
		return false;
	}

	OutDefinition = *Definition;
	return true;
}

bool UGT_ContentRegistry::FindCombatResultDef(FName ResultId, FGT_CombatResultDef& OutDefinition) const
{
	const FGT_CombatResultDef* Definition = CombatResultDefs.Find(ResultId);
	if (!Definition)
	{
		OutDefinition = FGT_CombatResultDef();
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

bool UGT_ContentRegistry::FindEventOptionDefForRoom(
	FName ContentId,
	FName RuleId,
	FName OptionId,
	FGT_EventOptionDef& OutDefinition,
	FString& OutFailureReason) const
{
	OutDefinition = FGT_EventOptionDef();
	OutFailureReason.Reset();

	FGT_RoomContentDef ContentDefinition;
	FGT_RoomRuleDef RuleDefinition;
	if (!FindRoomDefinitions(ContentId, RuleId, EGT_RoomBaseType::Event, ContentDefinition, RuleDefinition, OutFailureReason))
	{
		return false;
	}

	TArray<FName> AvailableOptionIds;
	BuildAvailableEventOptionIds(ContentDefinition, RuleDefinition, AvailableOptionIds);
	if (!AvailableOptionIds.Contains(OptionId))
	{
		OutFailureReason = FString::Printf(
			TEXT("OptionId=%s is not available for RoomContentId=%s RoomRuleId=%s. Available=%s."),
			*OptionId.ToString(),
			*ContentId.ToString(),
			*RuleId.ToString(),
			*BuildIdListText(AvailableOptionIds));
		return false;
	}

	if (!FindEventOptionDef(OptionId, OutDefinition))
	{
		OutFailureReason = FString::Printf(TEXT("OptionId=%s is not registered."), *OptionId.ToString());
		return false;
	}

	return true;
}

bool UGT_ContentRegistry::FindCombatResultDefForRoom(
	FName ContentId,
	FName RuleId,
	FName ResultId,
	FGT_CombatResultDef& OutDefinition,
	FString& OutFailureReason) const
{
	OutDefinition = FGT_CombatResultDef();
	OutFailureReason.Reset();

	FGT_RoomContentDef ContentDefinition;
	FGT_RoomRuleDef RuleDefinition;
	if (!FindRoomDefinitions(ContentId, RuleId, EGT_RoomBaseType::Combat, ContentDefinition, RuleDefinition, OutFailureReason))
	{
		return false;
	}

	TArray<FName> AvailableResultIds;
	BuildAvailableCombatResultIds(ContentDefinition, RuleDefinition, AvailableResultIds);
	if (!AvailableResultIds.Contains(ResultId))
	{
		OutFailureReason = FString::Printf(
			TEXT("ResultId=%s is not available for RoomContentId=%s RoomRuleId=%s. Available=%s."),
			*ResultId.ToString(),
			*ContentId.ToString(),
			*RuleId.ToString(),
			*BuildIdListText(AvailableResultIds));
		return false;
	}

	if (!FindCombatResultDef(ResultId, OutDefinition))
	{
		OutFailureReason = FString::Printf(TEXT("ResultId=%s is not registered."), *ResultId.ToString());
		return false;
	}

	return true;
}

bool UGT_ContentRegistry::GetEventOptionDefsForRoom(
	FName ContentId,
	FName RuleId,
	TArray<FGT_EventOptionDef>& OutDefinitions,
	FString& OutFailureReason) const
{
	OutDefinitions.Reset();
	OutFailureReason.Reset();

	FGT_RoomContentDef ContentDefinition;
	FGT_RoomRuleDef RuleDefinition;
	if (!FindRoomDefinitions(ContentId, RuleId, EGT_RoomBaseType::Event, ContentDefinition, RuleDefinition, OutFailureReason))
	{
		return false;
	}

	TArray<FName> AvailableOptionIds;
	BuildAvailableEventOptionIds(ContentDefinition, RuleDefinition, AvailableOptionIds);
	for (const FName OptionId : AvailableOptionIds)
	{
		FGT_EventOptionDef OptionDefinition;
		if (!FindEventOptionDef(OptionId, OptionDefinition))
		{
			OutFailureReason = FString::Printf(TEXT("OptionId=%s is configured but not registered."), *OptionId.ToString());
			OutDefinitions.Reset();
			return false;
		}

		OutDefinitions.Add(OptionDefinition);
	}

	return true;
}

bool UGT_ContentRegistry::GetCombatResultDefsForRoom(
	FName ContentId,
	FName RuleId,
	TArray<FGT_CombatResultDef>& OutDefinitions,
	FString& OutFailureReason) const
{
	OutDefinitions.Reset();
	OutFailureReason.Reset();

	FGT_RoomContentDef ContentDefinition;
	FGT_RoomRuleDef RuleDefinition;
	if (!FindRoomDefinitions(ContentId, RuleId, EGT_RoomBaseType::Combat, ContentDefinition, RuleDefinition, OutFailureReason))
	{
		return false;
	}

	TArray<FName> AvailableResultIds;
	BuildAvailableCombatResultIds(ContentDefinition, RuleDefinition, AvailableResultIds);
	for (const FName ResultId : AvailableResultIds)
	{
		FGT_CombatResultDef ResultDefinition;
		if (!FindCombatResultDef(ResultId, ResultDefinition))
		{
			OutFailureReason = FString::Printf(TEXT("ResultId=%s is configured but not registered."), *ResultId.ToString());
			OutDefinitions.Reset();
			return false;
		}

		OutDefinitions.Add(ResultDefinition);
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
	EventOptionDefs.Reset();
	CombatResultDefs.Reset();
}
