#include "Domains/Map/GT_MapGenerator.h"

bool UGT_MapGenerator::GenerateMap(const FGT_MapGenerationSpec& Spec, FGT_MapGenerationResult& OutResult)
{
	OutResult = FGT_MapGenerationResult();

	const EGT_MapMode RequestedMode = Spec.MapMode == EGT_MapMode::Unknown
		? EGT_MapMode::BasicDebug
		: Spec.MapMode;
	const int32 ResolvedWidth = Spec.Width > 0 ? Spec.Width : 10;
	const int32 ResolvedHeight = Spec.Height > 0 ? Spec.Height : 10;

	OutResult.MapMode = RequestedMode;
	OutResult.Seed = Spec.Seed;
	OutResult.Width = ResolvedWidth;
	OutResult.Height = ResolvedHeight;
	OutResult.TruthMap.Initialize(ResolvedWidth, ResolvedHeight, Spec.Seed);

	if (RequestedMode == EGT_MapMode::BasicDebug)
	{
		ApplyBasicDebugLayout(OutResult.TruthMap);
		OutResult.bSuccess = true;
		return true;
	}

	return false;
}

void UGT_MapGenerator::ApplyBasicDebugLayout(FGT_TruthMap& TruthMap)
{
	TruthMap.SetExit(TruthMap.Width - 1, TruthMap.Height - 1, true);
	TruthMap.SetMine(2, 2, true);
}
