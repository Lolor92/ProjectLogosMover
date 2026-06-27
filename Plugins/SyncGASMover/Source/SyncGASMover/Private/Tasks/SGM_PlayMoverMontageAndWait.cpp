// Copyright 2026 WeirdReflection. All Rights Reserved.

#include "Tasks/SGM_PlayMoverMontageAndWait.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Component/SGM_PawnComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GameFramework/Actor.h"
#include "MoverComponent.h"
#include "Tags/SGM_NativeTags.h"
#include "Tasks/SGM_ScaledAnimRootMotionLayeredMove.h"

USGM_PlayMoverMontageAndWait* USGM_PlayMoverMontageAndWait::PlayMoverMontageAndWait(UGameplayAbility* OwningAbility,
	FName TaskInstanceName, UMoverComponent* InMoverComponent, UAnimMontage* InMontage, float InPlayRate,
	FName InStartSection, float InStartTimeSeconds, float InRootMotionTranslationScale,
	ESGMRootMotionCollisionStopMode InRootMotionCollisionStopMode)
{
	USGM_PlayMoverMontageAndWait* Task =
		NewAbilityTask<USGM_PlayMoverMontageAndWait>(OwningAbility, TaskInstanceName);

	Task->MoverComponent = InMoverComponent;
	Task->Montage = InMontage;
	Task->PlayRate = InPlayRate;
	Task->StartSection = InStartSection;
	Task->StartTimeSeconds = InStartTimeSeconds;
	Task->RootMotionTranslationScale = InRootMotionTranslationScale;
	Task->RootMotionCollisionStopMode = InRootMotionCollisionStopMode;

	return Task;
}

void USGM_PlayMoverMontageAndWait::Activate()
{
	Super::Activate();

	if (!Ability)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast();
		}
		EndTask();
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast();
		}
		EndTask();
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	PawnComponent = AvatarActor ? AvatarActor->FindComponentByClass<USGM_PawnComponent>() : nullptr;
	ResolveMoverComponent(AvatarActor);

	if (!MoverComponent || !Montage)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast();
		}
		EndTask();
		return;
	}

	if (PawnComponent)
	{
		MeshComponent = PawnComponent->GetMontageMeshComponent();
	}

	if (!MeshComponent)
	{
		MeshComponent = ActorInfo->SkeletalMeshComponent.Get();
	}

	if (!MeshComponent)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast();
		}
		EndTask();
		return;
	}

	AnimInstance = MeshComponent->GetAnimInstance();
	if (!AnimInstance)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast();
		}
		EndTask();
		return;
	}

	if (!PlayScaledMoverMontage())
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast();
		}
		EndTask();
		return;
	}

	if (ActorInfo->IsNetAuthority() && PawnComponent)
	{
		PawnComponent->StartReplicatedMontage(
			Montage,
			PlayRate,
			StartTimeSeconds,
			StartSection,
			RootMotionTranslationScale);
	}

	FOnMontageBlendingOutStarted BlendOutDelegate;
	BlendOutDelegate.BindUObject(this, &USGM_PlayMoverMontageAndWait::OnMontageBlendingOut);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, Montage);

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &USGM_PlayMoverMontageAndWait::OnMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);

	bPlayedSuccessfully = true;
}

void USGM_PlayMoverMontageAndWait::ExternalCancel()
{
	StopReplicatedMontageIfNeeded();
	StopPlayingMontage();

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCancelled.Broadcast();
	}

	Super::ExternalCancel();
}

void USGM_PlayMoverMontageAndWait::OnDestroy(bool bInOwnerFinished)
{
	StopReplicatedMontageIfNeeded();

	if (AnimInstance && Montage)
	{
		FOnMontageBlendingOutStarted EmptyBlendOutDelegate;
		AnimInstance->Montage_SetBlendingOutDelegate(EmptyBlendOutDelegate, Montage);

		FOnMontageEnded EmptyEndDelegate;
		AnimInstance->Montage_SetEndDelegate(EmptyEndDelegate, Montage);
	}

	Super::OnDestroy(bInOwnerFinished);
}

void USGM_PlayMoverMontageAndWait::OnMontageBlendingOut(UAnimMontage* InMontage, bool bInterrupted)
{
	if (InMontage != Montage)
	{
		return;
	}

	if (!ShouldBroadcastAbilityTaskDelegates())
	{
		return;
	}

	if (bInterrupted)
	{
		OnInterrupted.Broadcast();
		return;
	}

	OnBlendOut.Broadcast();
}

void USGM_PlayMoverMontageAndWait::OnMontageEnded(UAnimMontage* InMontage, bool bInterrupted)
{
	if (InMontage != Montage)
	{
		return;
	}

	StopReplicatedMontageIfNeeded();

	if (!ShouldBroadcastAbilityTaskDelegates())
	{
		EndTask();
		return;
	}

	if (!bInterrupted)
	{
		OnCompleted.Broadcast();
	}

	EndTask();
}

bool USGM_PlayMoverMontageAndWait::StopPlayingMontage()
{
	if (!AnimInstance || !Montage)
	{
		return false;
	}

	if (!AnimInstance->Montage_IsPlaying(Montage))
	{
		return false;
	}

	AnimInstance->Montage_Stop(Montage->GetDefaultBlendOutTime(), Montage);
	return true;
}

bool USGM_PlayMoverMontageAndWait::PlayScaledMoverMontage()
{
	const float MontageLength = AnimInstance->Montage_Play(Montage, PlayRate, EMontagePlayReturnType::MontageLength,
		StartTimeSeconds, true);
	if (MontageLength <= 0.f)
	{
		return false;
	}

	if (StartSection != NAME_None)
	{
		AnimInstance->Montage_JumpToSection(StartSection, Montage);
	}

	FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(Montage);
	if (!MontageInstance)
	{
		return false;
	}

	if (PlayRate != 0.f && Montage->HasRootMotion())
	{
		MontageInstance->PushDisableRootMotion();
		QueueScaledRootMotionMove(MontageInstance->GetPosition());
	}

	return true;
}

void USGM_PlayMoverMontageAndWait::QueueScaledRootMotionMove(float StartingMontagePosition)
{
	MoverComponent->CancelFeaturesWithTag(TAG_SyncGASMover_RootMotion, true);

	TSharedPtr<FSGM_ScaledAnimRootMotionLayeredMove> AnimRootMotionMove =
		MakeShared<FSGM_ScaledAnimRootMotionLayeredMove>();
	AnimRootMotionMove->MontageState.Montage = Montage;
	AnimRootMotionMove->MontageState.PlayRate = PlayRate;
	AnimRootMotionMove->MontageState.StartingMontagePosition = StartingMontagePosition;
	AnimRootMotionMove->RootMotionTranslationScale = RootMotionTranslationScale;
	AnimRootMotionMove->RootMotionCollisionStopMode = RootMotionCollisionStopMode;
	AnimRootMotionMove->bIgnoreRetriggerCancellationWhileQueued = true;

	float RemainingUnscaledMontageSeconds = 0.f;
	if (PlayRate > 0.f)
	{
		RemainingUnscaledMontageSeconds = Montage->GetPlayLength() - StartingMontagePosition;
	}
	else
	{
		RemainingUnscaledMontageSeconds = StartingMontagePosition;
	}

	AnimRootMotionMove->DurationMs = (RemainingUnscaledMontageSeconds / FMath::Abs(PlayRate)) * 1000.f;

	MoverComponent->QueueLayeredMove(AnimRootMotionMove);
}

void USGM_PlayMoverMontageAndWait::ResolveMoverComponent(AActor* AvatarActor)
{
	if (MoverComponent)
	{
		return;
	}

	if (PawnComponent)
	{
		MoverComponent = PawnComponent->GetCharacterMoverComponent();
	}

	if (!MoverComponent && AvatarActor)
	{
		MoverComponent = AvatarActor->FindComponentByClass<UMoverComponent>();
	}
}
void USGM_PlayMoverMontageAndWait::StopReplicatedMontageIfNeeded()
{
	if (bReplicatedMontageStopped) return;

	if (!Ability) return;

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->IsNetAuthority()) return;

	if (!PawnComponent) return;

	PawnComponent->StopReplicatedMontage();
	bReplicatedMontageStopped = true;
}
