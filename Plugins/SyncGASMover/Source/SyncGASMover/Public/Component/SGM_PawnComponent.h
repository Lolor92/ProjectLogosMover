// Copyright 2026 WeirdReflection. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "MoverSimulationTypes.h"
#include "SGM_PawnComponent.generated.h"

class UAnimMontage;
class UCharacterMoverComponent;
class UMoverComponent;
class USceneComponent;
class USkeletalMeshComponent;
struct FMoverAuxStateContext;
struct FMoverSyncState;

USTRUCT(BlueprintType)
struct FSGMRepMontageState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY()
	float PlayRate = 1.f;

	UPROPERTY()
	float RootMotionTranslationScale = 1.f;

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
class SYNCGASMOVER_API USGM_PawnComponent : public UActorComponent, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	USGM_PawnComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, Category="Mover")
	void RequestMoveIntent(const FVector& MoveIntent);

	UFUNCTION(BlueprintCallable, Category="Mover")
	void ClearMoveIntent();
	
	UFUNCTION(BlueprintCallable, Category="Montage Replication")
	void StartReplicatedMontage(UAnimMontage* InMontage, float InPlayRate = 1.f, float InStartTimeSeconds = 0.f,
		FName InStartSection = NAME_None, float InRootMotionTranslationScale = 1.f);
	
	UFUNCTION(BlueprintCallable, Category="Montage Replication")
	void StopReplicatedMontage();
	
	UFUNCTION(BlueprintPure, Category="Mover")
	UCharacterMoverComponent* GetCharacterMoverComponent() const { return CharacterMoverComponent; }
	
	UFUNCTION(BlueprintPure, Category="Montage Replication")
	USkeletalMeshComponent* GetMontageMeshComponent() const { return MontageMeshComponent; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;
	
	void ResolveOwnerComponents();
	void BindToMoverPostFinalize();
	void UnbindFromMoverPostFinalize();
	
	UFUNCTION()
	void HandleMoverPostFinalize(const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState);
	
	UFUNCTION()
	void OnRep_RepMontageState();
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mover", meta=(UseComponentPicker))
	FComponentReference MoverComponentReference;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mover", meta=(UseComponentPicker))
	FComponentReference UpdatedComponentReference;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mover", meta=(UseComponentPicker))
	FComponentReference PrimaryVisualComponentReference;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mover|Input")
	bool bOverrideExternalMoverInput = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Montage Replication", meta=(UseComponentPicker))
	FComponentReference MontageMeshComponentReference;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMoverComponent> CharacterMoverComponent = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UMoverComponent> CachedMoverComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> UpdatedComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> PrimaryVisualComponent = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> MontageMeshComponent = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_RepMontageState)
	FSGMRepMontageState RepMontageState;

	FVector CachedMoveInputIntent = FVector::ZeroVector;
	int32 CachedFinalizedSimFrame = INDEX_NONE;
};
