#pragma once

#include "CoreMinimal.h"
#include "Pawn/PL_BasePawn.h"
#include "PL_NPCPawn.generated.h"

class UPL_AbilitySystemComponent;
class UPL_AttributeSet;

UCLASS()
class PROJECTLOGOS_API APL_NPCPawn : public APL_BasePawn
{
	GENERATED_BODY()

public:
	APL_NPCPawn();
	UPL_AttributeSet* GetOwnedAttributeSet() const { return OwnedAttributeSet; }

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability")
	TObjectPtr<UPL_AbilitySystemComponent> OwnedAbilitySystemComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability")
	TObjectPtr<UPL_AttributeSet> OwnedAttributeSet = nullptr;

private:
	void InitializeAbilitySystemFromSelf();
};
