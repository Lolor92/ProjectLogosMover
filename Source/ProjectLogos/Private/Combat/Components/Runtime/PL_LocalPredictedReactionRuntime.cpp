#include "Combat/Components/Runtime/PL_LocalPredictedReactionRuntime.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Actor.h"
#include "MoverComponent.h"

void UPL_LocalPredictedReactionRuntime::Tick(float DeltaTime)
{
	for (int32 Index = ActivePredictedReactions.Num() - 1; Index >= 0; --Index)
	{
		FPLPredictedReactionRecord& Record = ActivePredictedReactions[Index];

		if (!Record.TargetActor || !Record.TargetMover || !Record.Montage)
		{
			RemoveRecordAt(Index);
			continue;
		}

		TickPredictedReaction(Record, DeltaTime);

		if (Record.bIsReconciling && Record.ReconcileTimeRemaining <= 0.f)
		{
			RemoveRecordAt(Index);
		}
	}
}

TStatId UPL_LocalPredictedReactionRuntime::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPL_LocalPredictedReactionRuntime, STATGROUP_Tickables);
}

bool UPL_LocalPredictedReactionRuntime::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

int32 UPL_LocalPredictedReactionRuntime::RegisterPredictedReaction(
	AActor* TargetActor,
	UMoverComponent* PredictionFrameMover,
	UAnimMontage* Montage,
	int32 StartSimFrame,
	float PlayRate,
	float StartMontageTimeSeconds)
{
	if (!TargetActor || !PredictionFrameMover || !Montage || StartSimFrame == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	UMoverComponent* TargetMover = TargetActor->FindComponentByClass<UMoverComponent>();
	if (!TargetMover)
	{
		return INDEX_NONE;
	}

	FPLPredictedReactionRecord Record;
	Record.TargetActor = TargetActor;
	Record.TargetMover = TargetMover;
	Record.PredictionFrameMover = PredictionFrameMover;
	Record.Montage = Montage;
	Record.StartSimFrame = StartSimFrame;
	Record.Serial = NextSerial++;
	Record.PlayRate = PlayRate;
	Record.StartMontageTimeSeconds = StartMontageTimeSeconds;
	Record.LastEvaluatedMontageTime = StartMontageTimeSeconds;
	Record.OriginalBaseVisualTransform = TargetMover->GetBaseVisualComponentTransform();
	Record.CurrentVisualOffset = FTransform::Identity;

	ActivePredictedReactions.Add(Record);
	return Record.Serial;
}

bool UPL_LocalPredictedReactionRuntime::BeginReconcileMatchingPredictedReaction(
	AActor* TargetActor,
	UAnimMontage* Montage,
	int32 AuthoritativeStartSimFrame,
	int32 FrameTolerance,
	float ReconcileDuration)
{
	if (!TargetActor || !Montage || AuthoritativeStartSimFrame == INDEX_NONE)
	{
		return false;
	}

	for (FPLPredictedReactionRecord& Record : ActivePredictedReactions)
	{
		if (Record.TargetActor != TargetActor) continue;
		if (Record.Montage != Montage) continue;
		if (Record.StartSimFrame == INDEX_NONE) continue;

		const int32 FrameDelta = FMath::Abs(Record.StartSimFrame - AuthoritativeStartSimFrame);
		if (FrameDelta > FrameTolerance) continue;

		Record.bIsReconciling = true;
		Record.ReconcileDuration = FMath::Max(0.01f, ReconcileDuration);
		Record.ReconcileTimeRemaining = Record.ReconcileDuration;
		return true;
	}

	return false;
}

void UPL_LocalPredictedReactionRuntime::ClearPredictedReaction(int32 Serial)
{
	if (Serial == INDEX_NONE) return;

	for (int32 Index = 0; Index < ActivePredictedReactions.Num(); ++Index)
	{
		if (ActivePredictedReactions[Index].Serial != Serial) continue;

		RemoveRecordAt(Index);
		return;
	}
}

void UPL_LocalPredictedReactionRuntime::TickPredictedReaction(FPLPredictedReactionRecord& Record, float DeltaTime)
{
	if (!Record.PredictionFrameMover) return;

	const FMoverTimeStep& TimeStep = Record.PredictionFrameMover->GetLastTimeStep();
	const int32 CurrentSimFrame = TimeStep.ServerFrame;

	if (CurrentSimFrame == INDEX_NONE) return;

	if (Record.bIsReconciling)
	{
		const float Alpha = FMath::Clamp(DeltaTime / FMath::Max(Record.ReconcileTimeRemaining, KINDA_SMALL_NUMBER), 0.f, 1.f);

		const FVector NewTranslation = FMath::Lerp(
			Record.CurrentVisualOffset.GetTranslation(),
			FVector::ZeroVector,
			Alpha);

		const FQuat NewRotation = FQuat::Slerp(
			Record.CurrentVisualOffset.GetRotation(),
			FQuat::Identity,
			Alpha);

		const FVector NewScale = FMath::Lerp(
			Record.CurrentVisualOffset.GetScale3D(),
			FVector::OneVector,
			Alpha);

		Record.CurrentVisualOffset = FTransform(NewRotation, NewTranslation, NewScale);
		Record.TargetMover->SetBaseVisualComponentTransform(Record.CurrentVisualOffset * Record.OriginalBaseVisualTransform);

		Record.ReconcileTimeRemaining -= DeltaTime;
		return;
	}

	if (CurrentSimFrame < Record.StartSimFrame) return;

	const int32 FramesSinceStart = FMath::Max(0, CurrentSimFrame - Record.StartSimFrame);
	const float StepSeconds = TimeStep.StepMs * 0.001f;

	float CurrentMontageTime = Record.StartMontageTimeSeconds;
	CurrentMontageTime += FramesSinceStart * StepSeconds * Record.PlayRate;
	CurrentMontageTime = FMath::Clamp(CurrentMontageTime, 0.f, Record.Montage->GetPlayLength());

	if (CurrentMontageTime <= Record.LastEvaluatedMontageTime + KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FTransform DeltaRootMotion = Record.Montage->ExtractRootMotionFromTrackRange(
		Record.LastEvaluatedMontageTime,
		CurrentMontageTime);

	Record.LastEvaluatedMontageTime = CurrentMontageTime;
	Record.CurrentVisualOffset = DeltaRootMotion * Record.CurrentVisualOffset;

	Record.TargetMover->SetBaseVisualComponentTransform(Record.CurrentVisualOffset * Record.OriginalBaseVisualTransform);

	if (CurrentMontageTime >= Record.Montage->GetPlayLength() - KINDA_SMALL_NUMBER)
	{
		Record.bIsReconciling = true;
		Record.ReconcileDuration = 0.08f;
		Record.ReconcileTimeRemaining = Record.ReconcileDuration;
	}
}

void UPL_LocalPredictedReactionRuntime::RestoreRecordVisual(FPLPredictedReactionRecord& Record)
{
	if (!Record.TargetMover) return;

	Record.TargetMover->SetBaseVisualComponentTransform(Record.OriginalBaseVisualTransform);
}

void UPL_LocalPredictedReactionRuntime::RemoveRecordAt(int32 Index)
{
	if (!ActivePredictedReactions.IsValidIndex(Index)) return;

	RestoreRecordVisual(ActivePredictedReactions[Index]);
	ActivePredictedReactions.RemoveAt(Index);
}
