#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "PL_PlayMoverMontageAndWait.generated.h"

class UAnimInstance;
class UAnimMontage;
class UMoverComponent;
class UGameplayAbility;
class USkeletalMeshComponent;
class UPlayMoverMontageCallbackProxy;
class UPL_MontageReplicationComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPLMoverMontageSimpleDelegate);

UCLASS()
class PROJECTLOGOS_API UPL_PlayMoverMontageAndWait : public UAbilityTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FPLMoverMontageSimpleDelegate OnCompleted;

	UPROPERTY(BlueprintAssignable)
	FPLMoverMontageSimpleDelegate OnBlendOut;

	UPROPERTY(BlueprintAssignable)
	FPLMoverMontageSimpleDelegate OnInterrupted;

	UPROPERTY(BlueprintAssignable)
	FPLMoverMontageSimpleDelegate OnCancelled;

	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta=(DisplayName="Play Mover Montage And Wait",
		HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="true"))
	static UPL_PlayMoverMontageAndWait* PlayMoverMontageAndWait(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		UMoverComponent* InMoverComponent,
		UAnimMontage* InMontage,
		float InPlayRate = 1.f,
		FName InStartSection = NAME_None,
		float InStartTimeSeconds = 0.f);

	virtual void Activate() override;
	virtual void ExternalCancel() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

protected:
	void OnMontageBlendingOut(UAnimMontage* InMontage, bool bInterrupted);
	void OnMontageEnded(UAnimMontage* InMontage, bool bInterrupted);
	bool StopPlayingMontage();
	bool CreateMoverMontageProxy();
	void StopReplicatedMontageIfNeeded();

	UPROPERTY()
	TObjectPtr<UMoverComponent> MoverComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> MeshComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UAnimInstance> AnimInstance = nullptr;

	UPROPERTY()
	TObjectPtr<UPlayMoverMontageCallbackProxy> MoverMontageProxy = nullptr;

	UPROPERTY()
	TObjectPtr<UPL_MontageReplicationComponent> MontageReplicationComponent = nullptr;

	FName StartSection = NAME_None;
	float PlayRate = 1.f;
	float StartTimeSeconds = 0.f;

	bool bPlayedSuccessfully = false;
	bool bReplicatedMontageStopped = false;
};
