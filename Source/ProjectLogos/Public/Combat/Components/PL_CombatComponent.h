#pragma once

#include "CoreMinimal.h"
#include "Combat/Data/PL_AbilitySet.h"
#include "Combat/Data/PL_HitWindowTypes.h"
#include "Components/ActorComponent.h"
#include "PL_CombatComponent.generated.h"

class APL_BasePawn;
class UAbilitySystemComponent;
class UAnimNotifyState;
class USkeletalMeshComponent;

USTRUCT()
struct FPLActiveHitDetectionWindow
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> MeshComponent = nullptr;

	UPROPERTY()
	TObjectPtr<const UAnimNotifyState> NotifyState = nullptr;

	FName TraceSocketName = NAME_None;
	FPLHitWindowSettings HitWindowSettings;
	TSet<TWeakObjectPtr<AActor>> HitActorsThisWindow;
	bool bIsActive = false;

	void Reset()
	{
		MeshComponent = nullptr;
		NotifyState = nullptr;
		TraceSocketName = NAME_None;
		HitWindowSettings = FPLHitWindowSettings();
		HitActorsThisWindow.Reset();
		bIsActive = false;
	}
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTLOGOS_API UPL_CombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPL_CombatComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void InitializeCombat(APL_BasePawn* InPawn, UAbilitySystemComponent* InAbilitySystemComponent);
	void DeinitializeCombat();

	APL_BasePawn* GetOwningPawn() const { return OwningPawn; }
	UAbilitySystemComponent* GetAbilitySystemComponent() const { return AbilitySystemComponent; }

	bool BeginHitDetectionWindow(
		const UAnimNotifyState* NotifyState,
		USkeletalMeshComponent* MeshComp,
		FName TraceSocketName,
		const FPLHitWindowSettings& HitWindowSettings);

	void EndHitDetectionWindow(
		const UAnimNotifyState* NotifyState,
		USkeletalMeshComponent* MeshComp);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	TArray<TObjectPtr<UPL_AbilitySet>> DefaultAbilitySets;

private:
	void GrantDefaultAbilities();
	void ClearDefaultAbilities();

	FTransform GetHitTraceWorldTransform() const;
	void TickHitDetectionWindow();
	void ApplyHitWindowEffectsToActor(AActor* HitActor);

	UPROPERTY()
	TObjectPtr<APL_BasePawn> OwningPawn = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	FPLAbilitySetGrantedHandles DefaultAbilityHandles;
	FPLActiveHitDetectionWindow ActiveHitWindow;
};
