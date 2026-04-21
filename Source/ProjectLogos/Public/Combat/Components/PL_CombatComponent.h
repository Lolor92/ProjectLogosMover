#pragma once

#include "CoreMinimal.h"
#include "Combat/Data/PL_AbilitySet.h"
#include "Components/ActorComponent.h"
#include "PL_CombatComponent.generated.h"

class APL_BasePawn;
class UAbilitySystemComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTLOGOS_API UPL_CombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPL_CombatComponent();

	void InitializeCombat(APL_BasePawn* InPawn, UAbilitySystemComponent* InAbilitySystemComponent);
	void DeinitializeCombat();

	APL_BasePawn* GetOwningPawn() const { return OwningPawn; }
	UAbilitySystemComponent* GetAbilitySystemComponent() const { return AbilitySystemComponent; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	TArray<TObjectPtr<UPL_AbilitySet>> DefaultAbilitySets;

private:
	void GrantDefaultAbilities();
	void ClearDefaultAbilities();

	UPROPERTY()
	TObjectPtr<APL_BasePawn> OwningPawn = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	FPLAbilitySetGrantedHandles DefaultAbilityHandles;
};
