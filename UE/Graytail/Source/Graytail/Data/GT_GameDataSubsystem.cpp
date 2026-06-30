#include "Data/GT_GameDataSubsystem.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogGraytailGameData, Log, All);

namespace
{
	constexpr int32 SupportedSchemaVersion = 1;

	bool ValidateJsonValueShape(
		const TSharedPtr<FJsonValue>& Value,
		const FProperty* Property,
		const FString& Path,
		TArray<FString>& OutErrors);

	bool ValidateJsonObjectShape(
		const TSharedPtr<FJsonObject>& Object,
		const UStruct* StructType,
		const FString& Path,
		TArray<FString>& OutErrors)
	{
		if (!Object.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: expected an object."), *Path));
			return false;
		}

		bool bValid = true;
		TSet<FString> KnownFields;
		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			const FProperty* Property = *It;
			const FString FieldName = StructType->GetAuthoredNameForField(Property);
			const FString FieldPath = Path + TEXT(".") + FieldName;
			KnownFields.Add(FieldName);

			const TSharedPtr<FJsonValue>* FieldValue = Object->Values.Find(FieldName);
			if (!FieldValue)
			{
				OutErrors.Add(FString::Printf(TEXT("%s: required field is missing."), *FieldPath));
				bValid = false;
				continue;
			}
			bValid &= ValidateJsonValueShape(*FieldValue, Property, FieldPath, OutErrors);
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
		{
			if (!KnownFields.Contains(Pair.Key))
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s.%s: unknown field."),
					*Path,
					*Pair.Key));
				bValid = false;
			}
		}
		return bValid;
	}

	bool ValidateJsonValueShape(
		const TSharedPtr<FJsonValue>& Value,
		const FProperty* Property,
		const FString& Path,
		TArray<FString>& OutErrors)
	{
		if (!Value.IsValid() || Value->IsNull())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: null is not allowed."), *Path));
			return false;
		}

		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			if (Value->Type != EJson::Array)
			{
				OutErrors.Add(FString::Printf(TEXT("%s: expected an array."), *Path));
				return false;
			}

			bool bValid = true;
			const TArray<TSharedPtr<FJsonValue>>& Values = Value->AsArray();
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				bValid &= ValidateJsonValueShape(
					Values[Index],
					ArrayProperty->Inner,
					FString::Printf(TEXT("%s[%d]"), *Path, Index),
					OutErrors);
			}
			return bValid;
		}

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (Value->Type != EJson::Object)
			{
				OutErrors.Add(FString::Printf(TEXT("%s: expected an object."), *Path));
				return false;
			}
			return ValidateJsonObjectShape(Value->AsObject(), StructProperty->Struct, Path, OutErrors);
		}

		if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
		{
			if (Value->Type != EJson::Number)
			{
				OutErrors.Add(FString::Printf(TEXT("%s: expected a number."), *Path));
				return false;
			}

			const double Number = Value->AsNumber();
			const bool bIntegerInvalid = NumericProperty->IsInteger()
				&& (FMath::TruncToDouble(Number) != Number
					|| !NumericProperty->CanHoldValue(static_cast<int64>(Number)));
			if (!FMath::IsFinite(Number) || bIntegerInvalid)
			{
				OutErrors.Add(FString::Printf(TEXT("%s: number is outside the property range."), *Path));
				return false;
			}
			return true;
		}

		const bool bTypeMatches =
			(CastField<FBoolProperty>(Property) && Value->Type == EJson::Boolean)
			|| (CastField<FStrProperty>(Property) && Value->Type == EJson::String);
		if (!bTypeMatches)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: JSON type does not match the property."), *Path));
		}
		return bTypeMatches;
	}

	template <typename T>
	bool LoadJsonFile(const FString& Directory, const TCHAR* FileName, T& OutValue, TArray<FString>& OutErrors)
	{
		const FString Path = FPaths::Combine(Directory, FileName);
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: file is missing or unreadable."), FileName));
			return false;
		}

		TSharedPtr<FJsonObject> ParsedObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, ParsedObject) || !ParsedObject.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: invalid JSON syntax."), FileName));
			return false;
		}

		if (!ValidateJsonObjectShape(ParsedObject, T::StaticStruct(), FileName, OutErrors))
		{
			return false;
		}

		FText ConversionError;
		if (!FJsonObjectConverter::JsonObjectToUStruct(
			ParsedObject.ToSharedRef(),
			&OutValue,
			0,
			0,
			true,
			&ConversionError))
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s: incompatible fields: %s"),
				FileName,
				*ConversionError.ToString()));
			return false;
		}
		return true;
	}

	void ValidateVersion(const TCHAR* FileName, int32 Version, TArray<FString>& OutErrors)
	{
		if (Version != SupportedSchemaVersion)
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s: schemaVersion must be %d, got %d."),
				FileName,
				SupportedSchemaVersion,
				Version));
		}
	}

	bool IsProbability(float Value)
	{
		return Value >= 0.0f && Value <= 1.0f;
	}

	bool IsPercent(int32 Value)
	{
		return Value >= 0 && Value <= 100;
	}

	bool AddUniqueId(const FString& Domain, const FString& Id, TSet<FString>& Seen, TArray<FString>& OutErrors)
	{
		if (Id.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: id must not be empty."), *Domain));
			return false;
		}
		if (Seen.Contains(Id))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: duplicate id '%s'."), *Domain, *Id));
			return false;
		}
		Seen.Add(Id);
		return true;
	}

	bool IsKnownQuality(const FString& Quality, bool bAllowNone = false)
	{
		return Quality == TEXT("low")
			|| Quality == TEXT("common")
			|| Quality == TEXT("rare")
			|| Quality == TEXT("precious")
			|| Quality == TEXT("abnormal")
			|| (bAllowNone && Quality == TEXT("none"));
	}

	bool IsKnownEventId(const FString& Id)
	{
		return Id == TEXT("trader")
			|| Id == TEXT("dice")
			|| Id == TEXT("altar")
			|| Id == TEXT("trap");
	}

	bool IsKnownMonsterId(const FString& Id)
	{
		return Id == TEXT("slime")
			|| Id == TEXT("bat")
			|| Id == TEXT("drone")
			|| Id == TEXT("slimeling");
	}

	bool IsKnownEquipmentTrigger(const FString& Trigger)
	{
		return Trigger == TEXT("none")
			|| Trigger == TEXT("killPowerStack")
			|| Trigger == TEXT("protocolHeal")
			|| Trigger == TEXT("settleGoldBonus")
			|| Trigger == TEXT("chestBonusLoot");
	}

	bool IsKnownConsumableTrigger(const FString& Trigger)
	{
		return Trigger == TEXT("none") || Trigger == TEXT("luckyCoin");
	}

	void RequireIds(
		const FString& Domain,
		const TSet<FString>& Seen,
		std::initializer_list<const TCHAR*> RequiredIds,
		TArray<FString>& OutErrors)
	{
		for (const TCHAR* RequiredId : RequiredIds)
		{
			if (!Seen.Contains(RequiredId))
			{
				OutErrors.Add(FString::Printf(TEXT("%s: missing required id '%s'."), *Domain, RequiredId));
			}
		}
	}

	void RequireExactIds(
		const FString& Domain,
		const TSet<FString>& Seen,
		std::initializer_list<const TCHAR*> RequiredIds,
		TArray<FString>& OutErrors)
	{
		TSet<FString> Allowed;
		for (const TCHAR* RequiredId : RequiredIds)
		{
			Allowed.Add(RequiredId);
		}
		RequireIds(Domain, Seen, RequiredIds, OutErrors);
		for (const FString& Id : Seen)
		{
			if (!Allowed.Contains(Id))
			{
				OutErrors.Add(FString::Printf(TEXT("%s: unsupported id '%s'."), *Domain, *Id));
			}
		}
	}

	void ValidateManualLayout(
		const FGT_DifficultyBalanceRow& Row,
		TArray<FString>& OutErrors)
	{
		if (!Row.ManualLayout.bEnabled)
		{
			return;
		}

		TSet<int32> Occupied;
		auto AddCoord = [&Row, &Occupied, &OutErrors](
			const FString& Category,
			const FGT_DataCoord& Coord)
		{
			if (Coord.X < 0 || Coord.X >= Row.Width || Coord.Y < 0 || Coord.Y >= Row.Height)
			{
				OutErrors.Add(FString::Printf(
					TEXT("difficulties.json: '%s' %s coordinate (%d,%d) is out of bounds."),
					*Row.Id,
					*Category,
					Coord.X,
					Coord.Y));
				return;
			}

			const int32 Index = Coord.Y * Row.Width + Coord.X;
			if (Occupied.Contains(Index))
			{
				OutErrors.Add(FString::Printf(
					TEXT("difficulties.json: '%s' has overlapping content at (%d,%d)."),
					*Row.Id,
					Coord.X,
					Coord.Y));
				return;
			}
			Occupied.Add(Index);
		};

		AddCoord(TEXT("spawn"), Row.ManualLayout.Spawn);
		auto AddCoords = [&AddCoord](
			const FString& Category,
			const TArray<FGT_DataCoord>& Coords)
		{
			for (const FGT_DataCoord& Coord : Coords)
			{
				AddCoord(Category, Coord);
			}
		};
		AddCoords(TEXT("mine"), Row.ManualLayout.Mines);
		AddCoords(TEXT("monster room"), Row.ManualLayout.MonsterRooms);
		AddCoords(TEXT("chest room"), Row.ManualLayout.ChestRooms);
		AddCoords(TEXT("event room"), Row.ManualLayout.EventRooms);
		AddCoords(TEXT("exit"), Row.ManualLayout.Exits);

		if (Row.ManualLayout.Exits.IsEmpty())
		{
			OutErrors.Add(FString::Printf(
				TEXT("difficulties.json: '%s' manual layout must contain an exit."),
				*Row.Id));
		}
	}

	void ValidateRandomLayoutCapacity(
		const FGT_DifficultyBalanceRow& Row,
		TArray<FString>& OutErrors)
	{
		if (Row.ManualLayout.bEnabled)
		{
			return;
		}
		if (Row.RandomExitCount <= 0)
		{
			OutErrors.Add(FString::Printf(
				TEXT("difficulties.json: '%s' random layout must contain an exit."),
				*Row.Id));
			return;
		}

		const int32 TotalCells = Row.Width * Row.Height;
		const int32 SafeDiameter = Row.SpawnSafeRadius * 2 + 1;
		const int32 MaxSafeCells =
			FMath::Min(Row.Width, SafeDiameter) * FMath::Min(Row.Height, SafeDiameter);
		const int32 MinCandidates = TotalCells - MaxSafeCells;
		const int32 MineCount = FMath::RoundToInt(TotalCells * Row.MineDensity);
		const int32 MonsterCount = FMath::Max(2, FMath::RoundToInt(TotalCells * Row.MonsterRoomRatio));
		const int32 ChestCount = FMath::Clamp(
			FMath::RoundToInt(TotalCells * Row.ChestRoomRatio),
			2,
			5);
		const int32 EventCount = FMath::Max(1, FMath::RoundToInt(TotalCells * Row.EventRoomRatio));
		const int32 RequiredCandidates =
			MineCount + MonsterCount + ChestCount + EventCount + Row.RandomExitCount;
		if (RequiredCandidates > MinCandidates)
		{
			OutErrors.Add(FString::Printf(
				TEXT("difficulties.json: '%s' cannot guarantee room and exit placement."),
				*Row.Id));
		}
	}

	void ValidateDropTable(const FString& Domain, const TArray<FGT_DropThresholdConfig>& Table, TArray<FString>& OutErrors)
	{
		if (Table.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: dropTable must not be empty."), *Domain));
			return;
		}

		int32 Previous = 0;
		for (const FGT_DropThresholdConfig& Entry : Table)
		{
			if (Entry.MaxRoll <= Previous || Entry.MaxRoll > 100)
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: maxRoll values must be strictly increasing in [1,100]."),
					*Domain));
				break;
			}
			if (!IsKnownQuality(Entry.Quality, true))
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: unknown quality '%s'."),
					*Domain,
					*Entry.Quality));
			}
			Previous = Entry.MaxRoll;
		}
		if (Table.Last().MaxRoll != 100)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: final maxRoll must be 100."), *Domain));
		}
	}

	void ValidateCore(const FGT_CoreBalanceFile& File, TArray<FString>& OutErrors)
	{
		ValidateVersion(TEXT("core.json"), File.SchemaVersion, OutErrors);
		if (File.Player.BaseHp <= 0 || File.Player.BasePower <= 0)
		{
			OutErrors.Add(TEXT("core.json: player baseHp and basePower must be positive."));
		}
		if (File.Combat.MineDamage <= 0
			|| File.Combat.MineDamageFloor <= 0
			|| File.Combat.MineDamageFloor > File.Combat.MineDamage
			|| File.Combat.EnemyPowerMin <= 0
			|| File.Combat.EnemyPowerMax < File.Combat.EnemyPowerMin
			|| File.Combat.MonsterGoldMin < 0
			|| File.Combat.MonsterGoldMax < File.Combat.MonsterGoldMin
			|| File.Combat.PowerGainPerKill <= 0
			|| File.Combat.PowerGainCap <= 0
			|| File.Combat.DefaultMonsterHpBase <= 0
			|| File.Combat.MonsterDamageMin <= 0)
		{
			OutErrors.Add(TEXT("core.json: combat values contain an invalid range."));
		}
		if (File.Protocol.MaxPressure <= 0
			|| File.Protocol.ExplorePressure < 0
			|| File.Protocol.MinePressure < 0
			|| File.Protocol.CombatKillPressure < 0
			|| File.Protocol.Thresholds.IsEmpty())
		{
			OutErrors.Add(TEXT("core.json: protocol values contain an invalid range."));
		}

		TSet<int32> Levels;
		int32 PreviousPressure = File.Protocol.MaxPressure + 1;
		for (int32 Index = 0; Index < File.Protocol.Thresholds.Num(); ++Index)
		{
			const FGT_ProtocolThresholdConfig& Threshold = File.Protocol.Thresholds[Index];
			if (Threshold.Pressure < 0
				|| Threshold.Pressure > File.Protocol.MaxPressure
				|| Threshold.Level < 1
				|| Threshold.Level > 5
				|| Threshold.Level != Index + 1
				|| Threshold.Pressure >= PreviousPressure
				|| Levels.Contains(Threshold.Level))
			{
				OutErrors.Add(TEXT("core.json: protocol thresholds are invalid or contain duplicate levels."));
				break;
			}
			Levels.Add(Threshold.Level);
			PreviousPressure = Threshold.Pressure;
		}
		if (File.Protocol.Thresholds.Num() != 5
			|| File.Protocol.Thresholds.IsEmpty()
			|| File.Protocol.Thresholds.Last().Pressure != 0)
		{
			OutErrors.Add(TEXT("core.json: protocol thresholds must define levels 1-5 in descending pressure order and end at zero."));
		}
	}

	void ValidateDifficulties(const FGT_DifficultiesBalanceFile& File, TArray<FString>& OutErrors)
	{
		ValidateVersion(TEXT("difficulties.json"), File.SchemaVersion, OutErrors);
		TSet<FString> Seen;
		for (const FGT_DifficultyBalanceRow& Row : File.Difficulties)
		{
			AddUniqueId(TEXT("difficulties.json difficulties"), Row.Id, Seen, OutErrors);
			if (Row.Width <= 0
				|| Row.Height <= 0
				|| Row.SpawnSafeRadius < 0
				|| Row.RandomExitCount < 0
				|| !IsProbability(Row.MineDensity)
				|| !IsProbability(Row.MonsterRoomRatio)
				|| !IsProbability(Row.EventRoomRatio)
				|| !IsProbability(Row.ChestRoomRatio))
			{
				OutErrors.Add(FString::Printf(TEXT("difficulties.json: '%s' has an invalid range."), *Row.Id));
			}
			ValidateManualLayout(Row, OutErrors);
			ValidateRandomLayoutCapacity(Row, OutErrors);
		}

		RequireIds(
			TEXT("difficulties.json"),
			Seen,
			{ TEXT("tutorial"), TEXT("easy"), TEXT("standard"), TEXT("hard"),
				TEXT("veteran"), TEXT("elite"), TEXT("nightmare") },
			OutErrors);
	}

	void ValidateMonsters(const FGT_MonstersBalanceFile& File, TArray<FString>& OutErrors)
	{
		ValidateVersion(TEXT("monsters.json"), File.SchemaVersion, OutErrors);
		TSet<FString> Seen;
		int32 SpawnWeightTotal = 0;
		for (const FGT_MonsterBalanceRow& Row : File.Monsters)
		{
			AddUniqueId(TEXT("monsters.json monsters"), Row.Id, Seen, OutErrors);
			if (!IsKnownMonsterId(Row.Id))
			{
				OutErrors.Add(FString::Printf(TEXT("monsters.json: unknown monster id '%s'."), *Row.Id));
			}
			if (Row.HpBase <= 0
				|| Row.MoveSpeed < 0.0f
				|| Row.DamageMult <= 0.0f
				|| Row.AttackRadius < 0.0f
				|| Row.PlayerAttackRange <= 0.0f
				|| Row.IdleDuration < 0.0f
				|| Row.WarningDuration < 0.0f
				|| Row.ActiveDuration < 0.0f
				|| Row.CooldownDuration < 0.0f
				|| Row.IdealDistance < 0.0f
				|| Row.AimDuration < 0.0f
				|| Row.AttackInterval < 0.0f
				|| Row.SpreadCount < 0
				|| Row.SpreadHalfAngleDeg < 0.0f
				|| Row.SpreadHalfAngleDeg > 180.0f
				|| Row.LaserDuration < 0.0f
				|| Row.LaserTickInterval < 0.0f
				|| Row.LaserTurnRateDeg < 0.0f
				|| Row.AimTurnRateDeg < 0.0f
				|| Row.KiteStrength < 0.0f
				|| Row.WanderWeight < 0.0f
				|| Row.SpawnWeight < 0
				|| Row.SplitCount < 0
				|| Row.SplitPowerMultiplier < 0.0f)
			{
				OutErrors.Add(FString::Printf(TEXT("monsters.json: '%s' has an invalid range."), *Row.Id));
			}
			if (Row.Id == TEXT("slimeling") && Row.SpawnWeight != 0)
			{
				OutErrors.Add(TEXT("monsters.json: slimeling is split-only and must have spawnWeight 0."));
			}
			SpawnWeightTotal += Row.SpawnWeight;
		}
		if (SpawnWeightTotal <= 0)
		{
			OutErrors.Add(TEXT("monsters.json: at least one monster must have a positive spawnWeight."));
		}
		RequireIds(
			TEXT("monsters.json"),
			Seen,
			{ TEXT("slime"), TEXT("bat"), TEXT("drone"), TEXT("slimeling") },
			OutErrors);
	}

	void ValidateItems(const FGT_ItemsBalanceFile& File, TArray<FString>& OutErrors)
	{
		ValidateVersion(TEXT("items.json"), File.SchemaVersion, OutErrors);
		TSet<FString> ItemIds;
		for (const FGT_ItemBalanceRow& Row : File.Items)
		{
			AddUniqueId(TEXT("items.json items"), Row.Id, ItemIds, OutErrors);
			if (Row.Value < 0 || !IsKnownQuality(Row.Quality))
			{
				OutErrors.Add(FString::Printf(TEXT("items.json: '%s' has invalid value or quality."), *Row.Id));
			}
		}
		RequireExactIds(
			TEXT("items.json items"),
			ItemIds,
			{
				TEXT("broken_copper_wire"), TEXT("dim_capacitor"), TEXT("old_gear"),
				TEXT("broken_terminal"), TEXT("dead_battery"), TEXT("old_gauge"),
				TEXT("damaged_circuit"), TEXT("static_lens"), TEXT("blackbox_tag"),
				TEXT("data_disk"), TEXT("whisper_wick"), TEXT("fluorescent_shard"),
				TEXT("sealed_core_shard"), TEXT("anomaly_core_shard"),
				TEXT("emergency_bandage"), TEXT("lucky_coin")
			},
			OutErrors);

		TSet<FString> PoolQualities;
		for (const FGT_ItemDropPoolConfig& Pool : File.DropPools)
		{
			AddUniqueId(TEXT("items.json dropPools"), Pool.Quality, PoolQualities, OutErrors);
			if (!IsKnownQuality(Pool.Quality) || Pool.ItemIds.IsEmpty())
			{
				OutErrors.Add(FString::Printf(TEXT("items.json: pool '%s' is invalid or empty."), *Pool.Quality));
			}
			for (const FString& ItemId : Pool.ItemIds)
			{
				if (!ItemIds.Contains(ItemId))
				{
					OutErrors.Add(FString::Printf(
						TEXT("items.json: pool '%s' references unknown item '%s'."),
						*Pool.Quality,
						*ItemId));
				}
			}
		}
		RequireExactIds(
			TEXT("items.json dropPools"),
			PoolQualities,
			{ TEXT("low"), TEXT("common"), TEXT("rare"), TEXT("precious"), TEXT("abnormal") },
			OutErrors);
	}

	void ValidateLootEvents(const FGT_LootEventsBalanceFile& File, TArray<FString>& OutErrors)
	{
		ValidateVersion(TEXT("loot_events.json"), File.SchemaVersion, OutErrors);
		if (File.Search.BaseMin < 0
			|| File.Search.BaseMax < File.Search.BaseMin
			|| File.Search.AdjacentDivisor <= 0
			|| File.Search.GoldCap < File.Search.BaseMax
			|| File.Search.HighAdjacentAtLeast < 0
			|| File.Search.HighAdjacentRareBonus < 0)
		{
			OutErrors.Add(TEXT("loot_events.json: search values contain an invalid range."));
		}
		if (File.Chest.BaseMin < 0
			|| File.Chest.BaseMax < File.Chest.BaseMin
			|| File.Chest.GoldCap < File.Chest.BaseMax
			|| File.Chest.MinItems <= 0
			|| File.Chest.MaxItems < File.Chest.MinItems)
		{
			OutErrors.Add(TEXT("loot_events.json: chest values contain an invalid range."));
		}
		ValidateDropTable(TEXT("loot_events.json search"), File.Search.DropTable, OutErrors);
		ValidateDropTable(TEXT("loot_events.json chest"), File.Chest.DropTable, OutErrors);

		if (File.Gambler.Bet <= 0
			|| File.Gambler.LoseMaxRoll < 1
			|| File.Gambler.BigWinRoll <= File.Gambler.LoseMaxRoll
			|| File.Gambler.BigWinRoll > 6
			|| File.Gambler.SmallWinNet < 0
			|| File.Gambler.BigWinNet < File.Gambler.SmallWinNet)
		{
			OutErrors.Add(TEXT("loot_events.json: gambler values contain an invalid range."));
		}
		if (File.AltarSteps.IsEmpty())
		{
			OutErrors.Add(TEXT("loot_events.json: altarSteps must not be empty."));
		}
		for (const FGT_AltarStepConfig& Step : File.AltarSteps)
		{
			if (Step.HpCost <= 0 || Step.RewardGold < 0 || !IsKnownQuality(Step.RewardQuality))
			{
				OutErrors.Add(TEXT("loot_events.json: altar step contains an invalid range or quality."));
				break;
			}
		}
		if (File.Trap.PowerRequirement <= 0
			|| File.Trap.SuccessGold < 0
			|| File.Trap.FailHpLoss <= 0
			|| File.Trap.FailPressure < 0
			|| !IsPercent(File.Trader.BaseSalePercent)
			|| !IsPercent(File.Flee.GoldDropPercent)
			|| !IsPercent(File.Flee.ItemDropPercent)
			|| File.Flee.DropSalt < 0
			|| !IsPercent(File.LuckyCoin.GoldChancePercent)
			|| File.LuckyCoin.SafeGoldReward < 0
			|| File.LuckyCoin.RevealCount < 0)
		{
			OutErrors.Add(TEXT("loot_events.json: event values contain an invalid range."));
		}

		TSet<FString> EventIds;
		int32 WeightTotal = 0;
		for (const FGT_EventWeightConfig& Weight : File.EventWeights)
		{
			AddUniqueId(TEXT("loot_events.json eventWeights"), Weight.Id, EventIds, OutErrors);
			if (!IsKnownEventId(Weight.Id))
			{
				OutErrors.Add(FString::Printf(TEXT("loot_events.json: unknown event id '%s'."), *Weight.Id));
			}
			if (Weight.Weight <= 0)
			{
				OutErrors.Add(FString::Printf(TEXT("loot_events.json: event '%s' weight must be positive."), *Weight.Id));
			}
			WeightTotal += Weight.Weight;
		}
		if (WeightTotal != 100)
		{
			OutErrors.Add(FString::Printf(TEXT("loot_events.json: event weights must total 100, got %d."), WeightTotal));
		}
		RequireIds(
			TEXT("loot_events.json eventWeights"),
			EventIds,
			{ TEXT("trader"), TEXT("dice"), TEXT("altar"), TEXT("trap") },
			OutErrors);
	}

	template <typename T>
	void ValidateCatalogRows(
		const FString& Domain,
		const TArray<T>& Rows,
		TSet<FString>& OutIds,
		TArray<FString>& OutErrors)
	{
		for (const T& Row : Rows)
		{
			AddUniqueId(Domain, Row.Id, OutIds, OutErrors);
			if (Row.Price < 0)
			{
				OutErrors.Add(FString::Printf(TEXT("%s: '%s' price must not be negative."), *Domain, *Row.Id));
			}
		}
	}

	void ValidateMetaCatalog(
		const FGT_MetaCatalogBalanceFile& File,
		const FGT_ItemsBalanceFile& Items,
		TArray<FString>& OutErrors)
	{
		ValidateVersion(TEXT("meta_catalog.json"), File.SchemaVersion, OutErrors);
		if (File.MaxEquipped <= 0 || File.RecentRecoveryMax <= 0)
		{
			OutErrors.Add(TEXT("meta_catalog.json: limits must be positive."));
		}

		TSet<FString> EquipIds;
		TSet<FString> TalentIds;
		TSet<FString> ConsumableIds;
		ValidateCatalogRows(TEXT("meta_catalog.json equipment"), File.Equipment, EquipIds, OutErrors);
		ValidateCatalogRows(TEXT("meta_catalog.json talents"), File.Talents, TalentIds, OutErrors);
		ValidateCatalogRows(TEXT("meta_catalog.json consumables"), File.Consumables, ConsumableIds, OutErrors);
		RequireExactIds(
			TEXT("meta_catalog.json equipment"),
			EquipIds,
			{
				TEXT("armor"), TEXT("whetstone"), TEXT("medkit"), TEXT("insulated_gloves"),
				TEXT("compass"), TEXT("backpack"), TEXT("anomaly_fang"),
				TEXT("lockdown_crystal"), TEXT("company_badge"), TEXT("salvage_magnet")
			},
			OutErrors);
		RequireExactIds(
			TEXT("meta_catalog.json talents"),
			TalentIds,
			{
				TEXT("talent_map"), TEXT("talent_mine"), TEXT("talent_monster"),
				TEXT("talent_extract"), TEXT("talent_event")
			},
			OutErrors);
		RequireExactIds(
			TEXT("meta_catalog.json consumables"),
			ConsumableIds,
			{ TEXT("emergency_bandage"), TEXT("lucky_coin") },
			OutErrors);

		for (const FGT_EquipBalanceRow& Equipment : File.Equipment)
		{
			const bool bInvalidRange =
				Equipment.BonusHp < 0
				|| Equipment.BonusPower < 0
				|| Equipment.MineDamageReduce < 0
				|| !IsPercent(Equipment.SearchBonus)
				|| Equipment.TriggerCap < 0
				|| Equipment.TriggerAmount < 0;
			const bool bInvalidTriggerValues =
				(Equipment.Trigger == TEXT("none")
					&& (Equipment.TriggerCap != 0 || Equipment.TriggerAmount != 0))
				|| ((Equipment.Trigger == TEXT("killPowerStack")
						|| Equipment.Trigger == TEXT("protocolHeal"))
					&& (Equipment.TriggerCap <= 0 || Equipment.TriggerAmount <= 0))
				|| (Equipment.Trigger == TEXT("settleGoldBonus")
					&& (Equipment.TriggerCap != 0
						|| Equipment.TriggerAmount <= 0
						|| !IsPercent(Equipment.TriggerAmount)))
				|| (Equipment.Trigger == TEXT("chestBonusLoot")
					&& (Equipment.TriggerCap != 0 || Equipment.TriggerAmount <= 0));
			if (!IsKnownEquipmentTrigger(Equipment.Trigger)
				|| bInvalidRange
				|| bInvalidTriggerValues)
			{
				OutErrors.Add(FString::Printf(
					TEXT("meta_catalog.json: equipment '%s' has invalid effects or trigger '%s'."),
					*Equipment.Id,
					*Equipment.Trigger));
			}
		}

		for (const FGT_TalentBalanceRow& Talent : File.Talents)
		{
			if (Talent.MineDamageReduce < 0
				|| Talent.MonsterFleeBonus < 0
				|| !IsPercent(Talent.FailureGoldBonus)
				|| !IsPercent(Talent.TradePrice))
			{
				OutErrors.Add(FString::Printf(
					TEXT("meta_catalog.json: talent '%s' has invalid effects."),
					*Talent.Id));
			}
		}

		TSet<FString> ItemIds;
		for (const FGT_ItemBalanceRow& Item : Items.Items)
		{
			ItemIds.Add(Item.Id);
		}
		for (const FGT_ConsumableBalanceRow& Consumable : File.Consumables)
		{
			if (Consumable.MaxCarry <= 0
				|| Consumable.Heal < 0
				|| !ItemIds.Contains(Consumable.Id)
				|| !IsKnownConsumableTrigger(Consumable.Trigger))
			{
				OutErrors.Add(FString::Printf(
					TEXT("meta_catalog.json: consumable '%s' has invalid values, trigger, or no matching item."),
					*Consumable.Id));
			}
		}
	}
}

bool FGT_GameDataLoader::LoadFromDirectory(
	const FString& Directory,
	FGT_GameDataSnapshot& OutSnapshot,
	TArray<FString>& OutErrors)
{
	OutSnapshot = FGT_GameDataSnapshot();
	OutErrors.Reset();

	bool bFilesLoaded = true;
	bFilesLoaded &= LoadJsonFile(Directory, TEXT("core.json"), OutSnapshot.Core, OutErrors);
	bFilesLoaded &= LoadJsonFile(Directory, TEXT("difficulties.json"), OutSnapshot.Difficulties, OutErrors);
	bFilesLoaded &= LoadJsonFile(Directory, TEXT("monsters.json"), OutSnapshot.Monsters, OutErrors);
	bFilesLoaded &= LoadJsonFile(Directory, TEXT("items.json"), OutSnapshot.Items, OutErrors);
	bFilesLoaded &= LoadJsonFile(Directory, TEXT("loot_events.json"), OutSnapshot.LootEvents, OutErrors);
	bFilesLoaded &= LoadJsonFile(Directory, TEXT("meta_catalog.json"), OutSnapshot.MetaCatalog, OutErrors);
	if (!bFilesLoaded)
	{
		return false;
	}

	ValidateCore(OutSnapshot.Core, OutErrors);
	ValidateDifficulties(OutSnapshot.Difficulties, OutErrors);
	ValidateMonsters(OutSnapshot.Monsters, OutErrors);
	ValidateItems(OutSnapshot.Items, OutErrors);
	ValidateLootEvents(OutSnapshot.LootEvents, OutErrors);
	ValidateMetaCatalog(OutSnapshot.MetaCatalog, OutSnapshot.Items, OutErrors);
	return OutErrors.IsEmpty();
}

void UGT_GameDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadFromDirectory(GetDefaultDataDirectory());
}

bool UGT_GameDataSubsystem::ReloadFromDirectory(const FString& Directory, bool bLogErrors)
{
	FGT_GameDataSnapshot Candidate;
	TArray<FString> CandidateErrors;
	bReady = FGT_GameDataLoader::LoadFromDirectory(Directory, Candidate, CandidateErrors);
	if (bReady)
	{
		Snapshot = MoveTemp(Candidate);
		Errors.Reset();
		++Revision;
		UE_LOG(LogGraytailGameData, Display, TEXT("Loaded balance data from %s."), *Directory);
	}
	else
	{
		Errors = MoveTemp(CandidateErrors);
		if (bLogErrors)
		{
			for (const FString& Error : Errors)
			{
				UE_LOG(LogGraytailGameData, Error, TEXT("%s"), *Error);
			}
		}
	}
	return bReady;
}

bool UGT_GameDataSubsystem::IsReady() const
{
	return bReady;
}

const FGT_GameDataSnapshot* UGT_GameDataSubsystem::GetSnapshot() const
{
	return bReady ? &Snapshot : nullptr;
}

uint64 UGT_GameDataSubsystem::GetRevision() const
{
	return Revision;
}

FString UGT_GameDataSubsystem::GetErrorText() const
{
	return FString::Join(Errors, TEXT("\n"));
}

FString UGT_GameDataSubsystem::GetDefaultDataDirectory()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Graytail/Data/Balance"));
}

namespace GT_GameData
{
	const UGT_GameDataSubsystem* GetSubsystem()
	{
		return GEngine ? GEngine->GetEngineSubsystem<UGT_GameDataSubsystem>() : nullptr;
	}

	const FGT_GameDataSnapshot* GetSnapshot()
	{
		const UGT_GameDataSubsystem* Subsystem = GetSubsystem();
		return Subsystem ? Subsystem->GetSnapshot() : nullptr;
	}

	uint64 GetRevision()
	{
		const UGT_GameDataSubsystem* Subsystem = GetSubsystem();
		return Subsystem ? Subsystem->GetRevision() : 0;
	}

	bool IsReady()
	{
		const UGT_GameDataSubsystem* Subsystem = GetSubsystem();
		return Subsystem && Subsystem->IsReady();
	}

	FString GetErrorText()
	{
		const UGT_GameDataSubsystem* Subsystem = GetSubsystem();
		return Subsystem ? Subsystem->GetErrorText() : TEXT("Game data subsystem is unavailable.");
	}
}
