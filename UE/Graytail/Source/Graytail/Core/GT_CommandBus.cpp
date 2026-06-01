#include "Core/GT_CommandBus.h"

void UGT_CommandBus::SubmitCommand(const FGT_Command& Command)
{
	PendingCommands.Add(Command);
}

void UGT_CommandBus::ClearPendingCommands()
{
	PendingCommands.Reset();
}

int32 UGT_CommandBus::GetPendingCommandCount() const
{
	return PendingCommands.Num();
}

const TArray<FGT_Command>& UGT_CommandBus::GetPendingCommands() const
{
	return PendingCommands;
}
