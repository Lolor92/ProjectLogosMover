// Copyright 2026 WeirdReflection. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Tasks/SGM_ScaledAnimRootMotionLayeredMove.h"
#include "SGM_PlayMoverMontageAndWait.generated.h"

class UAnimInstance;
class UAnimMontage;
class AActor;
class UMoverComponent;
class UGameplayAbility;
class USkeletalMeshComponent;
class USGM_PawnComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSGMMoverMontageSimpleDelegate);

UCLASS()
class SYNCGASMOVER_API USGM_PlayMoverMontageAndWait : public UAbilityTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FSGMMoverMontageSimpleDelegate OnCompleted;

	UPROPERTY(BlueprintAssignable)
	FSGMMoverMontageSimpleDelegate OnBlendOut;

	UPROPERTY(BlueprintAssignable)
	FSGMMoverMontageSimpleDelegate OnInterrupted;

	UPROPERTY(BlueprintAssignable)
	FSGMMoverMontageSimpleDelegate OnCancelled;

	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta=(DisplayName="Play Mover Montage And Wait",
		HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="true",
		AdvancedDisplay="InMoverComponent"))
	static USGM_PlayMoverMontageAndWait* PlayMoverMontageAndWait(UGameplayAbility* OwningAbility,
		FName TaskInstanceName, UMoverComponent* InMoverComponent = nullptr, UAnimMontage* InMontage = nullptr,
		float InPlayRate = 1.f, FName InStartSection = NAME_None, float InStartTimeSeconds = 0.f,
		float InRootMotionTranslationScale = 1.f,
		ESGMRootMotionCollisionStopMode InRootMotionCollisionStopMode = ESGMRootMotionCollisionStopMode::None);

	virtual void Activate() override;
	virtual void ExternalCancel() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

protected:
	void OnMontageBlendingOut(UAnimMontage* InMontage, bool bInterrupted);
	void OnMontageEnded(UAnimMontage* InMontage, bool bInterrupted);
	bool StopPlayingMontage();
	bool PlayScaledMoverMontage();
	void QueueScaledRootMotionMove(float StartingMontagePosition);
	void StopReplicatedMontageIfNeeded();
	void ResolveMoverComponent(AActor* AvatarActor);

	UPROPERTY()
	TObjectPtr<UMoverComponent> MoverComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> MeshComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UAnimInstance> AnimInstance = nullptr;

	UPROPERTY()
	TObjectPtr<USGM_PawnComponent> PawnComponent = nullptr;

	FName StartSection = NAME_None;
	float PlayRate = 1.f;
	float StartTimeSeconds = 0.f;
	float RootMotionTranslationScale = 1.f;
	ESGMRootMotionCollisionStopMode RootMotionCollisionStopMode = ESGMRootMotionCollisionStopMode::None;

	bool bPlayedSuccessfully = false;
	bool bReplicatedMontageStopped = false;
};