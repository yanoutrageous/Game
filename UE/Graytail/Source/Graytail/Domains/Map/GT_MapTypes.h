#pragma once

#include "CoreMinimal.h"
#include "GT_MapTypes.generated.h"

UENUM(BlueprintType)
enum class EGT_RoomBaseType : uint8
{
	Unknown UMETA(DisplayName = "Unknown"),
	Normal UMETA(DisplayName = "Normal"),
	Mine UMETA(DisplayName = "Mine"),
	Exit UMETA(DisplayName = "Exit")
};

UENUM(BlueprintType)
enum class EGT_IntelReliabilityState : uint8
{
	Unknown UMETA(DisplayName = "Unknown"),
	Accurate UMETA(DisplayName = "Accurate"),
	OffsetPossible UMETA(DisplayName = "Offset Possible"),
	Distorted UMETA(DisplayName = "Distorted"),
	Stale UMETA(DisplayName = "Stale"),
	Hidden UMETA(DisplayName = "Hidden")
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_TruthCell
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Map")
	int32 X = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Map")
	int32 Y = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Map")
	EGT_RoomBaseType RoomBaseType = EGT_RoomBaseType::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Map")
	bool bHasMine = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Map")
	bool bIsExit = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Map")
	bool bResolved = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Map")
	bool bTriggered = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Map")
	FName RoomDefId = NAME_None;

	FGT_TruthCell() = default;

	FGT_TruthCell(int32 InX, int32 InY)
		: X(InX)
		, Y(InY)
	{
	}
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_TruthMap
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Map")
	int32 Width = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Map")
	int32 Height = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Map")
	int32 Seed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Map")
	TArray<FGT_TruthCell> Cells;

	void Initialize(int32 InWidth, int32 InHeight, int32 InSeed)
	{
		Width = FMath::Max(1, InWidth);
		Height = FMath::Max(1, InHeight);
		Seed = InSeed;

		Cells.Reset();
		Cells.SetNum(Width * Height);

		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				Cells[ToIndex(X, Y)] = FGT_TruthCell(X, Y);
			}
		}
	}

	void Reset()
	{
		Width = 0;
		Height = 0;
		Seed = 0;
		Cells.Reset();
	}

	bool IsValidCoord(int32 X, int32 Y) const
	{
		return X >= 0 && Y >= 0 && X < Width && Y < Height;
	}

	int32 ToIndex(int32 X, int32 Y) const
	{
		return IsValidCoord(X, Y) ? (Y * Width) + X : INDEX_NONE;
	}

	FGT_TruthCell* GetCell(int32 X, int32 Y)
	{
		const int32 Index = ToIndex(X, Y);
		return Cells.IsValidIndex(Index) ? &Cells[Index] : nullptr;
	}

	const FGT_TruthCell* GetCellConst(int32 X, int32 Y) const
	{
		const int32 Index = ToIndex(X, Y);
		return Cells.IsValidIndex(Index) ? &Cells[Index] : nullptr;
	}
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_IntelCell
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Intel")
	int32 X = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Intel")
	int32 Y = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Intel")
	bool bVisible = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Intel")
	bool bExplored = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Intel")
	bool bScanned = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Intel")
	int32 DisplayedNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Intel")
	FName MarkerState = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Intel")
	FName VisibleRoomIcon = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Intel")
	bool bStale = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graytail|Intel")
	EGT_IntelReliabilityState ReliabilityState = EGT_IntelReliabilityState::Unknown;

	FGT_IntelCell() = default;

	FGT_IntelCell(int32 InX, int32 InY)
		: X(InX)
		, Y(InY)
	{
	}
};

USTRUCT(BlueprintType)
struct GRAYTAIL_API FGT_IntelMap
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Intel")
	FName OwnerActorId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Intel")
	int32 Width = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Intel")
	int32 Height = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Graytail|Intel")
	TArray<FGT_IntelCell> Cells;

	void Initialize(int32 InWidth, int32 InHeight, FName InOwnerActorId)
	{
		OwnerActorId = InOwnerActorId;
		Width = FMath::Max(1, InWidth);
		Height = FMath::Max(1, InHeight);

		Cells.Reset();
		Cells.SetNum(Width * Height);

		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				Cells[ToIndex(X, Y)] = FGT_IntelCell(X, Y);
			}
		}
	}

	void Reset()
	{
		OwnerActorId = NAME_None;
		Width = 0;
		Height = 0;
		Cells.Reset();
	}

	bool IsValidCoord(int32 X, int32 Y) const
	{
		return X >= 0 && Y >= 0 && X < Width && Y < Height;
	}

	int32 ToIndex(int32 X, int32 Y) const
	{
		return IsValidCoord(X, Y) ? (Y * Width) + X : INDEX_NONE;
	}

	FGT_IntelCell* GetCell(int32 X, int32 Y)
	{
		const int32 Index = ToIndex(X, Y);
		return Cells.IsValidIndex(Index) ? &Cells[Index] : nullptr;
	}

	const FGT_IntelCell* GetCellConst(int32 X, int32 Y) const
	{
		const int32 Index = ToIndex(X, Y);
		return Cells.IsValidIndex(Index) ? &Cells[Index] : nullptr;
	}
};
