// Copyright ProjectLogos

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "PL_GameplayAbility.generated.h"


UCLASS()
class PROJECTLOGOS_API UPL_GameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPL_GameplayAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	TSubclassOf<UGameplayAbility> GetComboAbilityClass() const { return ComboAbilityClass; }
	float GetComboWindowDuration() const { return ComboWindowDuration; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Ability|Combo")
	TSubclassOf<UGameplayAbility> ComboAbilityClass = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Combo", meta = (ClampMin = "0.0", Units = "Seconds"))
	float ComboWindowDuration = 2.f;
};
