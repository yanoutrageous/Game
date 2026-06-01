#include "Save/GT_SaveGame.h"

#include "Core/GT_RunContext.h"

void UGT_SaveGame::Reset()
{
	Metadata = FGT_RunSaveMetadata();
	MapSnapshot = FGT_MapSaveSnapshot();
	ActorSnapshots.Reset();
}

void UGT_SaveGame::SetFromRunContext(const UGT_RunContext* RunContext)
{
	Reset();

	if (!IsValid(RunContext))
	{
		return;
	}

	Metadata.RunId = RunContext->GetRunId();
	Metadata.Seed = RunContext->GetSeed();
	Metadata.MapWidth = RunContext->GetMapWidth();
	Metadata.MapHeight = RunContext->GetMapHeight();
	Metadata.SavedAtUtc = FDateTime::UtcNow();

	const FGT_IntelMap& IntelMap = RunContext->GetPlayerIntelMap();
	MapSnapshot.Width = IntelMap.Width;
	MapSnapshot.Height = IntelMap.Height;
	MapSnapshot.Seed = RunContext->GetSeed();
	MapSnapshot.PlayerIntelCells = IntelMap.Cells;
}
