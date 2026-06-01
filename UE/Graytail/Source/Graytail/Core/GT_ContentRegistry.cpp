#include "Core/GT_ContentRegistry.h"

void UGT_ContentRegistry::RegisterContentId(FName ContentId)
{
	if (!ContentId.IsNone())
	{
		RegisteredContentIds.Add(ContentId);
	}
}

bool UGT_ContentRegistry::IsContentRegistered(FName ContentId) const
{
	return RegisteredContentIds.Contains(ContentId);
}

void UGT_ContentRegistry::ClearRegistry()
{
	RegisteredContentIds.Reset();
}
