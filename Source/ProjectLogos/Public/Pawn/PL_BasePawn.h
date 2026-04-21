#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "MoverSimulationTypes.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "GameFramework/Pawn.h"
#include "PL_BasePawn.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class UPL_CombatComponent;
class UCharacterMoverComponent;
class UCapsuleComponent;
class USkeletalMeshComponent;

UCLASS()
class PROJECTLOGOS_API APL_BasePawn : public APawn, public IAbilitySystemInterface, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	APL_BasePawn();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAttributeSet* GetAttributeSet() const;
	UCharacterMoverComponent* GetCharacterMoverComponent() const { return CharacterMoverComponent; }
	UPL_CombatComponent* GetCombatComponent() const { return CombatComponent; }
	USkeletalMeshComponent* GetMeshComponent() const { return MeshComponent; }

	UFUNCTION(BlueprintCallable, Category = "Mover")
	void RequestMoveIntent(const FVector& MoveIntent);

	UFUNCTION(BlueprintCallable, Category = "Mover")
	void ClearMoveIntent();

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

	void InitializeAbilitySystemReferences(AActor* OwnerActor, UAbilitySystemComponent* InAbilitySystemComponent,
		UAttributeSet* InAttributeSet);

	void ClearAbilitySystemReferences();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mover")
	TObjectPtr<UCharacterMoverComponent> CharacterMoverComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UPL_CombatComponent> CombatComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> MeshComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCapsuleComponent> CapsuleComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mover")
	TObjectPtr<UCommonLegacyMovementSettings> MovementSettings = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAttributeSet> AttributeSet = nullptr;

private:
	void ApplyConfiguredMovementSettings();

	FVector CachedMoveInputIntent = FVector::ZeroVector;
};
