#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Component/PL_MontagePlayPolicy.h"
#include "PL_MontageReplicationComponent.generated.h"

class UAnimMontage;
class UMoverComponent;
struct FMoverSyncState;
struct FMoverAuxStateContext;

USTRUCT(BlueprintType)
struct FPLRepMontageState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY()
	float PlayRate = 1.f;

	UPROPERTY()
	float StartMontageTimeSeconds = 0.f;

	UPROPERTY()
	FName StartSection = NAME_None;

	UPROPERTY()
	int32 StartSimFrame = INDEX_NONE;

	UPROPERTY()
	int32 Serial = 0;

	UPROPERTY()
	bool bIsPlaying = false;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTLOGOS_API UPL_MontageReplicationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPL_MontageReplicationComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="Montage Replication")
	int32 StartReplicatedMontage(UAnimMontage* InMontage, float InPlayRate = 1.f,
		float InStartTimeSeconds = 0.f, FName InStartSection = NAME_None);

	UFUNCTION(BlueprintCallable, Category="Montage Replication")
	void StopReplicatedMontageIfCurrent(int32 ExpectedSerial);
	
	bool CanStartMontageWithPolicy(const FPLMontagePlayPolicy& NewPolicy) const;
	void SetActiveMontagePolicy(const FPLMontagePlayPolicy& InPolicy);
	void ClearActiveMontagePolicy();

protected:
	UFUNCTION()
	void HandleMoverPostFinalize(const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState);

	void BindToMoverPostFinalize();
	void UnbindFromMoverPostFinalize();
	
	FPLMontagePlayPolicy ActiveMontagePolicy;
	bool bHasActiveMontagePolicy = false;

	UPROPERTY()
	TObjectPtr<UMoverComponent> CachedMoverComponent = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_RepMontageState)
	FPLRepMontageState RepMontageState;

	int32 CachedFinalizedSimFrame = INDEX_NONE;

	UFUNCTION()
	void OnRep_RepMontageState();
};
