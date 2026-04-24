#include "GAS/Task/PL_PlayMoverMontageAndWait.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Component/PL_MontageReplicationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "MoveLibrary/PlayMoverMontageCallbackProxy.h"
#include "MoverComponent.h"

UPL_PlayMoverMontageAndWait* UPL_PlayMoverMontageAndWait::PlayMoverMontageAndWait(UGameplayAbility* OwningAbility,
	FName TaskInstanceName, UMoverComponent* InMoverComponent, UAnimMontage* InMontage, float InPlayRate,
	FName InStartSection, float InStartTimeSeconds, FPLMontagePlayPolicy InPlayPolicy)
{
	UPL_PlayMoverMontageAndWait* Task = NewAbilityTask<UPL_PlayMoverMontageAndWait>(OwningAbility, TaskInstanceName);

	Task->MoverComponent = InMoverComponent;
	Task->Montage = InMontage;
	Task->PlayRate = InPlayRate;
	Task->StartSection = InStartSection;
	Task->StartTimeSeconds = InStartTimeSeconds;
	Task->PlayPolicy = InPlayPolicy;

	return Task;
}

void UPL_PlayMoverMontageAndWait::Activate()
{
	Super::Activate();

	if (!Ability)
	{
		if (ShouldBroadcastAbilityTaskDelegates()) OnCancelled.Broadcast();
		EndTask();
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		if (ShouldBroadcastAbilityTaskDelegates()) OnCancelled.Broadcast();
		EndTask();
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	MontageReplicationComponent = AvatarActor ? AvatarActor->FindComponentByClass<UPL_MontageReplicationComponent>() : nullptr;
	
	if (ActorInfo->IsNetAuthority() && MontageReplicationComponent)
	{
		if (!MontageReplicationComponent->CanStartMontageWithPolicy(PlayPolicy))
		{
			if (ShouldBroadcastAbilityTaskDelegates()) OnCancelled.Broadcast();
			EndTask();
			return;
		}
	}

	if (!MoverComponent)
	{
		if (ShouldBroadcastAbilityTaskDelegates()) OnCancelled.Broadcast();
		EndTask();
		return;
	}

	if (!Montage)
	{
		if (ShouldBroadcastAbilityTaskDelegates()) OnCancelled.Broadcast();
		EndTask();
		return;
	}

	MeshComponent = ActorInfo->SkeletalMeshComponent.Get();
	if (!MeshComponent)
	{
		if (ShouldBroadcastAbilityTaskDelegates()) OnCancelled.Broadcast();
		EndTask();
		return;
	}

	AnimInstance = MeshComponent->GetAnimInstance();
	if (!AnimInstance)
	{
		if (ShouldBroadcastAbilityTaskDelegates()) OnCancelled.Broadcast();
		EndTask();
		return;
	}

	if (!CreateMoverMontageProxy())
	{
		if (ShouldBroadcastAbilityTaskDelegates()) OnCancelled.Broadcast();
		EndTask();
		return;
	}

	FOnMontageBlendingOutStarted BlendOutDelegate;
	BlendOutDelegate.BindUObject(this, &UPL_PlayMoverMontageAndWait::OnMontageBlendingOut);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, Montage);

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UPL_PlayMoverMontageAndWait::OnMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);

	bPlayedSuccessfully = true;
}

void UPL_PlayMoverMontageAndWait::ExternalCancel()
{
	StopReplicatedMontage();
	StopPlayingMontage();

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCancelled.Broadcast();
	}

	Super::ExternalCancel();
}

void UPL_PlayMoverMontageAndWait::OnDestroy(bool bInOwnerFinished)
{
	StopReplicatedMontage();

	if (AnimInstance && Montage)
	{
		FOnMontageBlendingOutStarted EmptyBlendOutDelegate;
		AnimInstance->Montage_SetBlendingOutDelegate(EmptyBlendOutDelegate, Montage);

		FOnMontageEnded EmptyEndDelegate;
		AnimInstance->Montage_SetEndDelegate(EmptyEndDelegate, Montage);
	}

	Super::OnDestroy(bInOwnerFinished);
}

void UPL_PlayMoverMontageAndWait::OnMontageBlendingOut(UAnimMontage* InMontage, bool bInterrupted)
{
	if (InMontage != Montage) return;
	if (!ShouldBroadcastAbilityTaskDelegates()) return;

	if (bInterrupted)
	{
		OnInterrupted.Broadcast();
		return;
	}

	OnBlendOut.Broadcast();
}

void UPL_PlayMoverMontageAndWait::OnMontageEnded(UAnimMontage* InMontage, bool bInterrupted)
{
	if (InMontage != Montage) return;

	StopReplicatedMontage();

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

bool UPL_PlayMoverMontageAndWait::StopPlayingMontage()
{
	if (!AnimInstance || !Montage) return false;
	if (!AnimInstance->Montage_IsPlaying(Montage)) return false;

	AnimInstance->Montage_Stop(Montage->GetDefaultBlendOutTime(), Montage);
	return true;
}

bool UPL_PlayMoverMontageAndWait::CreateMoverMontageProxy()
{
	MoverMontageProxy = UPlayMoverMontageCallbackProxy::CreateProxyObjectForPlayMoverMontage(
		MoverComponent, Montage, PlayRate, StartTimeSeconds, StartSection);

	if (!MoverMontageProxy) return false;
	if (!AnimInstance->GetActiveInstanceForMontage(Montage))
	{
		MoverMontageProxy = nullptr;
		return false;
	}

	return true;
}

void UPL_PlayMoverMontageAndWait::StopReplicatedMontage()
{
	if (bReplicatedMontageStopped) return;
	if (!Ability) return;

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->IsNetAuthority()) return;
	bReplicatedMontageStopped = true;
	StartedRepMontageSerial = INDEX_NONE;
}
