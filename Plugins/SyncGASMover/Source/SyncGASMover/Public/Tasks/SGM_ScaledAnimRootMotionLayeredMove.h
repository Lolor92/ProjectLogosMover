// Copyright 2026 WeirdReflection. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/LayeredMoves/AnimRootMotionLayeredMove.h"
#include "SGM_ScaledAnimRootMotionLayeredMove.generated.h"

UENUM(BlueprintType)
enum class ESGMRootMotionCollisionStopMode : uint8
{
	None UMETA(DisplayName="None"),
	Capsule UMETA(DisplayName="Capsule"),
	Mesh UMETA(DisplayName="Mesh"),
	CapsuleOrMesh UMETA(DisplayName="Capsule Or Mesh")
};

USTRUCT(BlueprintType)
struct SYNCGASMOVER_API FSGM_ScaledAnimRootMotionLayeredMove : public FLayeredMove_AnimRootMotion
{
	GENERATED_BODY()

	FSGM_ScaledAnimRootMotionLayeredMove();
	virtual ~FSGM_ScaledAnimRootMotionLayeredMove() {}

	UPROPERTY(BlueprintReadWrite, Category = Mover)
	float RootMotionTranslationScale = 1.f;

	UPROPERTY(BlueprintReadWrite, Category = Mover)
	ESGMRootMotionCollisionStopMode RootMotionCollisionStopMode = ESGMRootMotionCollisionStopMode::None;

	bool bIgnoreRetriggerCancellationWhileQueued = false;

	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
		const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;

private:
	bool HasBlockingPawnCollision(const UMoverComponent* MoverComp, const FVector& WorldTranslation) const;
	bool IsRelevantCollisionHit(const FHitResult& Hit, const AActor* OwnerActor) const;
};

template<>
struct TStructOpsTypeTraits<FSGM_ScaledAnimRootMotionLayeredMove> : public TStructOpsTypeTraitsBase2<FSGM_ScaledAnimRootMotionLayeredMove>
{
	enum
	{
		WithCopy = true
	};
};
