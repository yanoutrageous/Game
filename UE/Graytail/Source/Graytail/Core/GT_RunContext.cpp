#include "Core/GT_RunContext.h"

void UGT_RunContext::InitializeRun(int32 InSeed, int32 InWidth, int32 InHeight)
{
	RunId = FGuid::NewGuid();
	Seed = InSeed;
	MapWidth = InWidth > 0 ? InWidth : 10;
	MapHeight = InHeight > 0 ? InHeight : 10;

	TruthMap.Initialize(MapWidth, MapHeight, Seed);
	PlayerIntelMap.Initialize(MapWidth, MapHeight, FName(TEXT("Player")));
}

void UGT_RunContext::ResetRun()
{
	RunId.Invalidate();
	Seed = 0;
	MapWidth = 0;
	MapHeight = 0;
	TruthMap.Reset();
	PlayerIntelMap.Reset();
}

FGuid UGT_RunContext::GetRunId() const
{
	return RunId;
}

int32 UGT_RunContext::GetSeed() const
{
	return Seed;
}

int32 UGT_RunContext::GetMapWidth() const
{
	return MapWidth;
}

int32 UGT_RunContext::GetMapHeight() const
{
	return MapHeight;
}

const FGT_TruthMap& UGT_RunContext::GetTruthMap() const
{
	return TruthMap;
}

const FGT_IntelMap& UGT_RunContext::GetPlayerIntelMap() const
{
	return PlayerIntelMap;
}
