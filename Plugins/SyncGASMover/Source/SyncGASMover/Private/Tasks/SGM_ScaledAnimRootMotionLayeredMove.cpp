// Copyright 2026 WeirdReflection. All Rights Reserved.

#include "Tasks/SGM_ScaledAnimRootMotionLayeredMove.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "MotionWarpingComponent.h"
#include "MoveLibrary/MovementUtilsTypes.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "Tags/SGM_NativeTags.h"

FSGM_ScaledAnimRootMotionLayeredMove::FSGM_ScaledAnimRootMotionLayeredMove()
{
	DurationMs = 0.f;
	MixMode = EMoveMixMode::OverrideAll;
	FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::SetVelocity;
	FinishVelocitySettings.SetVelocity = FVector::ZeroVector;

	MontageState.Montage = nullptr;
	MontageState.StartingMontagePosition = 0.f;
	MontageState.PlayRate = 1.f;
}

bool FSGM_ScaledAnimRootMotionLayeredMove::GenerateMove(const FMoverTickStartData& SimState,
	const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard,
	FProposedMove& OutProposedMove)
{
	if (!TimeStep.bIsResimulating)
	{
		bool bIsMontageStillPlaying = false;

		if (const USkeletalMeshComponent* MeshComp = Cast<USkeletalMeshComponent>(MoverComp->GetPrimaryVisualComponent()))
		{
			if (const UAnimInstance* MeshAnimInstance = MeshComp->GetAnimInstance())
			{
				bIsMontageStillPlaying = MontageState.Montage && MeshAnimInstance->Montage_IsPlaying(MontageState.Montage);
			}
		}

		if (!bIsMontageStillPlaying)
		{
			DurationMs = 0.f;
			return false;
		}
	}

	const float DeltaSeconds = TimeStep.StepMs / 1000.f;
	const FMoverDefaultSyncState* SyncState = SimState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (SyncState == nullptr)
	{
		return false;
	}

	const float SecondsSinceMontageStarted = (TimeStep.BaseSimTimeMs - StartSimTimeMs) / 1000.f;
	const float ScaledSecondsSinceMontageStarted = SecondsSinceMontageStarted * MontageState.PlayRate;

	const float ExtractionStartPosition = MontageState.StartingMontagePosition + ScaledSecondsSinceMontageStarted;
	const float ExtractionEndPosition = ExtractionStartPosition + (DeltaSeconds * MontageState.PlayRate);

	const FTransform UnscaledLocalRootMotion = MontageState.Montage
		? UMotionWarpingUtilities::ExtractRootMotionFromAnimation(MontageState.Montage, ExtractionStartPosition, ExtractionEndPosition)
		: FTransform::Identity;

	FMotionWarpingUpdateContext WarpingContext;
	WarpingContext.Animation = MontageState.Montage;
	WarpingContext.CurrentPosition = ExtractionEndPosition;
	WarpingContext.PreviousPosition = ExtractionStartPosition;
	WarpingContext.PlayRate = MontageState.PlayRate;
	WarpingContext.Weight = 1.f;

	const FTransform SimActorTransform =
		FTransform(SyncState->GetOrientation_WorldSpace().Quaternion(), SyncState->GetLocation_WorldSpace());

	float EffectiveTranslationScale = RootMotionTranslationScale;
	FTransform WorldSpaceRootMotion;

	if (RootMotionCollisionStopMode != ESGMRootMotionCollisionStopMode::None)
	{
		const FTransform UnscaledWorldSpaceRootMotion =
			MoverComp->ConvertLocalRootMotionToWorld(UnscaledLocalRootMotion, DeltaSeconds, &SimActorTransform, &WarpingContext);

		if (HasBlockingPawnCollision(MoverComp, UnscaledWorldSpaceRootMotion.GetTranslation()))
		{
			EffectiveTranslationScale = 0.f;
		}

		if (FMath::IsNearlyEqual(EffectiveTranslationScale, 1.f))
		{
			WorldSpaceRootMotion = UnscaledWorldSpaceRootMotion;
		}
	}

	if (WorldSpaceRootMotion.Equals(FTransform::Identity))
	{
		FTransform LocalRootMotion = UnscaledLocalRootMotion;
		if (!FMath::IsNearlyEqual(EffectiveTranslationScale, 1.f))
		{
			LocalRootMotion.SetTranslation(LocalRootMotion.GetTranslation() * EffectiveTranslationScale);
		}

		WorldSpaceRootMotion = MoverComp->ConvertLocalRootMotionToWorld(LocalRootMotion, DeltaSeconds, &SimActorTransform, &WarpingContext);
	}

	OutProposedMove = FProposedMove();
	OutProposedMove.MixMode = MixMode;

	if (DeltaSeconds > UE_KINDA_SMALL_NUMBER)
	{
		OutProposedMove.LinearVelocity = WorldSpaceRootMotion.GetTranslation() / DeltaSeconds;
		OutProposedMove.AngularVelocityDegrees = FMath::RadiansToDegrees(WorldSpaceRootMotion.GetRotation().ToRotationVector() / DeltaSeconds);
	}

	MontageState.CurrentPosition = ExtractionStartPosition;

	return true;
}

bool FSGM_ScaledAnimRootMotionLayeredMove::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	if (bIgnoreRetriggerCancellationWhileQueued && StartSimTimeMs < 0.0)
	{
		return false;
	}

	const FGameplayTag RootMotionTag = TAG_SyncGASMover_RootMotion.GetTag();
	return bExactMatch ? RootMotionTag.MatchesTagExact(TagToFind) : RootMotionTag.MatchesTag(TagToFind);
}

bool FSGM_ScaledAnimRootMotionLayeredMove::HasBlockingPawnCollision(const UMoverComponent* MoverComp, const FVector& WorldTranslation) const
{
	if (!MoverComp || WorldTranslation.IsNearlyZero())
	{
		return false;
	}

	AActor* OwnerActor = MoverComp->GetOwner();
	UPrimitiveComponent* UpdatedPrimitive = Cast<UPrimitiveComponent>(MoverComp->GetUpdatedComponent());
	UWorld* World = UpdatedPrimitive ? UpdatedPrimitive->GetWorld() : nullptr;
	if (!OwnerActor || !UpdatedPrimitive || !World || !UpdatedPrimitive->IsCollisionEnabled())
	{
		return false;
	}

	const FVector Start = UpdatedPrimitive->GetComponentLocation();
	const FVector End = Start + WorldTranslation;
	if (Start.Equals(End))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SGMRootMotionCollisionStop), false, OwnerActor);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);
	QueryParams.AddIgnoredActor(OwnerActor);

	TArray<FHitResult> Hits;
	const FCollisionShape CollisionShape = UpdatedPrimitive->GetCollisionShape(2.f);
	const bool bHit = World->SweepMultiByChannel(
		Hits,
		Start,
		End,
		UpdatedPrimitive->GetComponentQuat(),
		UpdatedPrimitive->GetCollisionObjectType(),
		CollisionShape,
		QueryParams,
		ResponseParams);

	if (!bHit)
	{
		return false;
	}

	for (const FHitResult& Hit : Hits)
	{
		if (IsRelevantCollisionHit(Hit, OwnerActor))
		{
			return true;
		}
	}

	return false;
}

bool FSGM_ScaledAnimRootMotionLayeredMove::IsRelevantCollisionHit(const FHitResult& Hit, const AActor* OwnerActor) const
{
	if (!Hit.bBlockingHit)
	{
		return false;
	}

	const AActor* HitActor = Hit.GetActor();
	const UPrimitiveComponent* HitComponent = Hit.GetComponent();
	if (!HitActor || HitActor == OwnerActor || !HitComponent)
	{
		return false;
	}

	if (!HitActor->IsA<APawn>())
	{
		return false;
	}

		const bool bIsCapsule = HitComponent->IsA<UCapsuleComponent>();
	const bool bIsMesh = HitComponent->IsA<UMeshComponent>();

	switch (RootMotionCollisionStopMode)
	{
	case ESGMRootMotionCollisionStopMode::Capsule:
		return bIsCapsule;
	case ESGMRootMotionCollisionStopMode::Mesh:
		return bIsMesh;
	case ESGMRootMotionCollisionStopMode::CapsuleOrMesh:
		return bIsCapsule || bIsMesh;
	case ESGMRootMotionCollisionStopMode::None:
	default:
		return false;
	}
}

FLayeredMoveBase* FSGM_ScaledAnimRootMotionLayeredMove::Clone() const
{
	FSGM_ScaledAnimRootMotionLayeredMove* CopyPtr = new FSGM_ScaledAnimRootMotionLayeredMove(*this);
	return CopyPtr;
}

void FSGM_ScaledAnimRootMotionLayeredMove::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Ar << RootMotionTranslationScale;
		uint8 CollisionStopModeAsByte = static_cast<uint8>(RootMotionCollisionStopMode);
	Ar << CollisionStopModeAsByte;
	RootMotionCollisionStopMode = static_cast<ESGMRootMotionCollisionStopMode>(CollisionStopModeAsByte);
}

UScriptStruct* FSGM_ScaledAnimRootMotionLayeredMove::GetScriptStruct() const
{
	return FSGM_ScaledAnimRootMotionLayeredMove::StaticStruct();
}

FString FSGM_ScaledAnimRootMotionLayeredMove::ToSimpleString() const
{
	return FString::Printf(TEXT("SGM_ScaledAnimRootMotion"));
}
