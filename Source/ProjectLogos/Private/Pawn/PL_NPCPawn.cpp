#include "Pawn/PL_NPCPawn.h"

#include "GAS/ASC/PL_AbilitySystemComponent.h"
#include "GAS/Attribute/PL_AttributeSet.h"

APL_NPCPawn::APL_NPCPawn()
{
	OwnedAbilitySystemComponent = CreateDefaultSubobject<UPL_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	OwnedAbilitySystemComponent->SetIsReplicated(true);
	OwnedAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	OwnedAttributeSet = CreateDefaultSubobject<UPL_AttributeSet>(TEXT("AttributeSet"));
}

void APL_NPCPawn::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilitySystemFromSelf();
}

void APL_NPCPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitializeAbilitySystemFromSelf();
}

void APL_NPCPawn::InitializeAbilitySystemFromSelf()
{
	InitializeAbilitySystemReferences(this, OwnedAbilitySystemComponent, OwnedAttributeSet);
}
