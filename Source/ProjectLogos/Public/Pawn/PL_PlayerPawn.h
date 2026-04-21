#pragma once

#include "CoreMinimal.h"
#include "Pawn/PL_BasePawn.h"
#include "PL_PlayerPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UPL_InputComponent;

UCLASS()
class PROJECTLOGOS_API APL_PlayerPawn : public APL_BasePawn
{
	GENERATED_BODY()

public:
	APL_PlayerPawn();
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USpringArmComponent> SpringArmComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UCameraComponent> CameraComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UPL_InputComponent> PawnInputComponent = nullptr;
	
private:
	void InitializeAbilitySystemFromPlayerState();
};
