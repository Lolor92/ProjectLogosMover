#pragma once

#include "CoreMinimal.h"
#include "PL_LocalPredictedReactionRuntime.generated.h"

class UAnimMontage;
class UMoverComponent;

USTRUCT()
struct FPLPredictedReactionRecord
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY()
	TObjectPtr<UMoverComponent> TargetMover = nullptr;

	UPROPERTY()
	TObjectPtr<UMoverComponent> PredictionFrameMover = nullptr;

	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY()
	int32 StartSimFrame = INDEX_NONE;

	UPROPERTY()
	int32 Serial = INDEX_NONE;

	UPROPERTY()
	float PlayRate = 1.f;

	UPROPERTY()
	float StartMontageTimeSeconds = 0.f;

	UPROPERTY()
	float LastEvaluatedMontageTime = 0.f;

	UPROPERTY()
	bool bIsReconciling = false;

	UPROPERTY()
	float ReconcileTimeRemaining = 0.f;

	UPROPERTY()
	float ReconcileDuration = 0.12f;

	FTransform OriginalBaseVisualTransform = FTransform::Identity;
	FTransform CurrentVisualOffset = FTransform::Identity;
};

UCLASS()
class PROJECTLOGOS_API UPL_LocalPredictedReactionRuntime : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	int32 RegisterPredictedReaction(
		AActor* TargetActor,
		UMoverComponent* PredictionFrameMover,
		UAnimMontage* Montage,
		int32 StartSimFrame,
		float PlayRate = 1.f,
		float StartMontageTimeSeconds = 0.f);

	bool BeginReconcileMatchingPredictedReaction(
		AActor* TargetActor,
		UAnimMontage* Montage,
		int32 AuthoritativeStartSimFrame,
		int32 FrameTolerance = 2,
		float ReconcileDuration = 0.12f);

	void ClearPredictedReaction(int32 Serial);

private:
	void TickPredictedReaction(FPLPredictedReactionRecord& Record, float DeltaTime);
	void RestoreRecordVisual(FPLPredictedReactionRecord& Record);
	void RemoveRecordAt(int32 Index);

	int32 NextSerial = 1;
	TArray<FPLPredictedReactionRecord> ActivePredictedReactions;
};
