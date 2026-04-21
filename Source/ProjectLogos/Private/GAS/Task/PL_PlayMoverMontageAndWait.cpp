#include "GAS/Task/PL_PlayMoverMontageAndWait.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

UPL_PlayMoverMontageAndWait* UPL_PlayMoverMontageAndWait::PlayMoverMontageAndWait(UGameplayAbility* OwningAbility,
	FName TaskInstanceName, UCharacterMoverComponent* InMoverComponent, UAnimMontage* InMontage, float InPlayRate,
	FName InStartSection, float InStartTimeSeconds)
{
	UPL_PlayMoverMontageAndWait* Task = NewAbilityTask<UPL_PlayMoverMontageAndWait>(OwningAbility, TaskInstanceName);

	Task->MoverComponent = InMoverComponent;
	Task->Montage = InMontage;
	Task->PlayRate = InPlayRate;
	Task->StartSection = InStartSection;
	Task->StartTimeSeconds = InStartTimeSeconds;

	return Task;
}

void UPL_PlayMoverMontageAndWait::Activate()
{
	Super::Activate();

	if (!Ability)
	{
		UE_LOG(LogTemp, Warning, TEXT("PL_PlayMoverMontageAndWait: Activate failed, Ability is null."));
		if (ShouldBroadcastAbilityTaskDelegates()) OnCancelled.Broadcast();
		EndTask();
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		UE_LOG(LogTemp, Warning, TEXT("PL_PlayMoverMontageAndWait: Activate failed, ActorInfo is null."));
		if (ShouldBroadcastAbilityTaskDelegates()) OnCancelled.Broadcast();
		EndTask();
		return;
	}

	if (!MoverComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("PL_PlayMoverMontageAndWait: Activate failed, MoverComponent is null."));
		if (ShouldBroadcastAbilityTaskDelegates()) OnCancelled.Broadcast();
		EndTask();
		return;
	}

	if (!Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("PL_PlayMoverMontageAndWait: Activate failed, Montage is null."));
		if (ShouldBroadcastAbilityTaskDelegates()) OnCancelled.Broadcast();
		EndTask();
		return;
	}

	MeshComponent = ActorInfo->SkeletalMeshComponent.Get();
	if (!MeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("PL_PlayMoverMontageAndWait: Activate failed, SkeletalMeshComponent is null."));
		if (ShouldBroadcastAbilityTaskDelegates()) OnCancelled.Broadcast();
		EndTask();
		return;
	}

	AnimInstance = MeshComponent->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("PL_PlayMoverMontageAndWait: Activate failed, AnimInstance is null."));
		if (ShouldBroadcastAbilityTaskDelegates()) OnCancelled.Broadcast();
		EndTask();
		return;
	}
	
	const float PlayedLength = AnimInstance->Montage_Play(Montage, PlayRate,
	EMontagePlayReturnType::MontageLength, StartTimeSeconds, false);

	if (PlayedLength <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("PL_PlayMoverMontageAndWait: Activate failed, Montage_Play failed."));
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

	if (StartSection != NAME_None)
	{
		AnimInstance->Montage_JumpToSection(StartSection, Montage);
	}

	bPlayedSuccessfully = true;
}

void UPL_PlayMoverMontageAndWait::ExternalCancel()
{
	StopPlayingMontage();

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCancelled.Broadcast();
	}

	Super::ExternalCancel();
}

void UPL_PlayMoverMontageAndWait::OnDestroy(bool bInOwnerFinished)
{
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
