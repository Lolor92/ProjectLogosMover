#include "Combat/Components/PL_CombatComponent.h"

#include "AbilitySystemComponent.h"
#include "Pawn/PL_BasePawn.h"

UPL_CombatComponent::UPL_CombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPL_CombatComponent::InitializeCombat(APL_BasePawn* InPawn, UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (AbilitySystemComponent && AbilitySystemComponent != InAbilitySystemComponent)
	{
		DeinitializeCombat();
	}

	OwningPawn = InPawn;
	AbilitySystemComponent = InAbilitySystemComponent;

	GrantDefaultAbilities();
}

void UPL_CombatComponent::DeinitializeCombat()
{
	ClearDefaultAbilities();

	OwningPawn = nullptr;
	AbilitySystemComponent = nullptr;
}

void UPL_CombatComponent::GrantDefaultAbilities()
{
	if (!AbilitySystemComponent || !OwningPawn || !OwningPawn->HasAuthority())
	{
		return;
	}

	for (const UPL_AbilitySet* AbilitySet : DefaultAbilitySets)
	{
		if (!AbilitySet)
		{
			continue;
		}

		AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &DefaultAbilityHandles, OwningPawn);
	}
}

void UPL_CombatComponent::ClearDefaultAbilities()
{
	if (!AbilitySystemComponent || !OwningPawn || !OwningPawn->HasAuthority())
	{
		return;
	}

	DefaultAbilityHandles.TakeFromAbilitySystem(AbilitySystemComponent);
}
