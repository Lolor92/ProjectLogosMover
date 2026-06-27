// Copyright 2026 WeirdReflection. All Rights Reserved.

#include "Component/SGM_PawnComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "MoverComponent.h"

USGM_PawnComponent::USGM_PawnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USGM_PawnComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, RepMontageState);
}

void USGM_PawnComponent::BeginPlay()
{
	Super::BeginPlay();
	
	ResolveOwnerComponents();

	if (CharacterMoverComponent)
	{
		if (UpdatedComponent)
		{
			CharacterMoverComponent->SetUpdatedComponent(UpdatedComponent);
		}

		if (PrimaryVisualComponent)
		{
			CharacterMoverComponent->SetPrimaryVisualComponent(PrimaryVisualComponent);
		}
	}

	BindToMoverPostFinalize();
}

void USGM_PawnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromMoverPostFinalize();

	Super::EndPlay(EndPlayReason);
}

void USGM_PawnComponent::ResolveOwnerComponents()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	CharacterMoverComponent = Cast<UCharacterMoverComponent>(MoverComponentReference.GetComponent(OwnerActor));
	if (!CharacterMoverComponent)
	{
		CharacterMoverComponent = OwnerActor->FindComponentByClass<UCharacterMoverComponent>();
	}

	CachedMoverComponent = Cast<UMoverComponent>(MoverComponentReference.GetComponent(OwnerActor));
	if (!CachedMoverComponent)
	{
		CachedMoverComponent = OwnerActor->FindComponentByClass<UMoverComponent>();
	}

	if (!CharacterMoverComponent)
	{
		CharacterMoverComponent = Cast<UCharacterMoverComponent>(CachedMoverComponent);
	}

	UpdatedComponent = Cast<USceneComponent>(UpdatedComponentReference.GetComponent(OwnerActor));
	PrimaryVisualComponent = Cast<USceneComponent>(PrimaryVisualComponentReference.GetComponent(OwnerActor));

	MontageMeshComponent = Cast<USkeletalMeshComponent>(MontageMeshComponentReference.GetComponent(OwnerActor));
	if (!MontageMeshComponent)
	{
		MontageMeshComponent = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
	}
}

void USGM_PawnComponent::BindToMoverPostFinalize()
{
	ResolveOwnerComponents();

	if (!CachedMoverComponent) return;

	CachedMoverComponent->OnPostFinalize.RemoveDynamic(this, &USGM_PawnComponent::HandleMoverPostFinalize);
	CachedMoverComponent->OnPostFinalize.AddDynamic(this, &USGM_PawnComponent::HandleMoverPostFinalize);
}

void USGM_PawnComponent::UnbindFromMoverPostFinalize()
{
	if (!CachedMoverComponent)
	{
		return;
	}

	CachedMoverComponent->OnPostFinalize.RemoveDynamic(this, &USGM_PawnComponent::HandleMoverPostFinalize);
	CachedMoverComponent = nullptr;
}

void USGM_PawnComponent::HandleMoverPostFinalize(const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	if (!CachedMoverComponent) return;

	CachedFinalizedSimFrame = CachedMoverComponent->GetLastTimeStep().ServerFrame;
}

void USGM_PawnComponent::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	FCharacterDefaultInputs* ExistingCharacterInputs =
		InputCmdResult.InputCollection.FindMutableDataByType<FCharacterDefaultInputs>();

	const bool bHasOwnMoveIntent = !CachedMoveInputIntent.IsNearlyZero();
	const bool bHasExternalMoveIntent = ExistingCharacterInputs
		&& ExistingCharacterInputs->GetMoveInputType() != EMoveInputType::None
		&& !ExistingCharacterInputs->GetMoveInput().IsNearlyZero();

	if (!bOverrideExternalMoverInput && !bHasOwnMoveIntent && bHasExternalMoveIntent)
	{
		return;
	}

	FCharacterDefaultInputs& CharacterInputs = ExistingCharacterInputs
		? *ExistingCharacterInputs
		: InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	CharacterInputs.OrientationIntent = FVector::ZeroVector;

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;

	if (!PlayerController)
	{
		CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, CachedMoveInputIntent);
		return;
	}
	
	const FRotator ControlRotation = PlayerController->GetControlRotation();
	const FRotator YawRotation(0.f, ControlRotation.Yaw, 0.f);
	const FVector WorldMoveIntent = YawRotation.RotateVector(CachedMoveInputIntent);

	CharacterInputs.ControlRotation = ControlRotation;
	CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, WorldMoveIntent);

	if (WorldMoveIntent.SizeSquared2D() > 0.01f)
	{
		CharacterInputs.OrientationIntent = YawRotation.Vector();
	}
}

void USGM_PawnComponent::RequestMoveIntent(const FVector& MoveIntent)
{
	CachedMoveInputIntent = MoveIntent;
}

void USGM_PawnComponent::ClearMoveIntent()
{
	CachedMoveInputIntent = FVector::ZeroVector;
}

void USGM_PawnComponent::StartReplicatedMontage(UAnimMontage* InMontage, float InPlayRate,
	float InStartTimeSeconds, FName InStartSection, float InRootMotionTranslationScale)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	if (!InMontage) return;

	ResolveOwnerComponents();

	int32 StartFrame = CachedFinalizedSimFrame;

	if (StartFrame == INDEX_NONE && CachedMoverComponent)
	{
		StartFrame = CachedMoverComponent->GetLastTimeStep().ServerFrame;
	}

	RepMontageState.Montage = InMontage;
	RepMontageState.PlayRate = InPlayRate;
	RepMontageState.RootMotionTranslationScale = InRootMotionTranslationScale;
	RepMontageState.StartMontageTimeSeconds = InStartTimeSeconds;
	RepMontageState.StartSection = InStartSection;
	RepMontageState.StartSimFrame = StartFrame;
	RepMontageState.Serial++;
	RepMontageState.bIsPlaying = true;
}

void USGM_PawnComponent::StopReplicatedMontage()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	RepMontageState.StartSimFrame = INDEX_NONE;
	RepMontageState.Serial++;
	RepMontageState.bIsPlaying = false;
}

void USGM_PawnComponent::OnRep_RepMontageState()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || OwnerActor->GetLocalRole() != ROLE_SimulatedProxy) return;

	ResolveOwnerComponents();

	if (!MontageMeshComponent) return;

	UAnimInstance* AnimInstance = MontageMeshComponent->GetAnimInstance();
	if (!AnimInstance) return;

	if (!RepMontageState.bIsPlaying)
	{
		if (RepMontageState.Montage && AnimInstance->Montage_IsPlaying(RepMontageState.Montage))
		{
			AnimInstance->Montage_Stop(RepMontageState.Montage->GetDefaultBlendOutTime(), RepMontageState.Montage);
		}
		return;
	}

	if (!RepMontageState.Montage || !CachedMoverComponent || RepMontageState.StartSimFrame == INDEX_NONE)
	{
		return;
	}

	const FMoverTimeStep& TimeStep = CachedMoverComponent->GetLastTimeStep();

	const int32 CurrentSimFrame = CachedFinalizedSimFrame != INDEX_NONE
		? CachedFinalizedSimFrame
		: TimeStep.ServerFrame;

	if (CurrentSimFrame == INDEX_NONE) return;

	const int32 FramesSinceStart = FMath::Max(0, CurrentSimFrame - RepMontageState.StartSimFrame);
	const float StepSeconds = TimeStep.StepMs * 0.001f;

	float MontagePosition = RepMontageState.StartMontageTimeSeconds;
	if (RepMontageState.StartSection != NAME_None)
	{
		const int32 SectionIndex = RepMontageState.Montage->GetSectionIndex(RepMontageState.StartSection);
		if (SectionIndex != INDEX_NONE)
		{
			float SectionStartTime = 0.f;
			float SectionEndTime = 0.f;
			RepMontageState.Montage->GetSectionStartAndEndTime(SectionIndex, SectionStartTime, SectionEndTime);
			MontagePosition = SectionStartTime;
		}
	}

	MontagePosition += FramesSinceStart * StepSeconds * RepMontageState.PlayRate;
	MontagePosition = FMath::Clamp(MontagePosition, 0.f, RepMontageState.Montage->GetPlayLength());

	const float PlayedLength = AnimInstance->Montage_Play(RepMontageState.Montage,
		RepMontageState.PlayRate, EMontagePlayReturnType::MontageLength, MontagePosition, false);

	if (PlayedLength <= 0.f) return;

	if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(RepMontageState.Montage))
	{
		MontageInstance->PushDisableRootMotion();
	}
}
