#include "Component/PL_MontageReplicationComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "MoverComponent.h"
#include "Net/UnrealNetwork.h"

UPL_MontageReplicationComponent::UPL_MontageReplicationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UPL_MontageReplicationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, RepMontageState);
}

void UPL_MontageReplicationComponent::BeginPlay()
{
	Super::BeginPlay();
	BindToMoverPostFinalize();
}

void UPL_MontageReplicationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromMoverPostFinalize();
	Super::EndPlay(EndPlayReason);
}

int32 UPL_MontageReplicationComponent::StartReplicatedMontage(
	UAnimMontage* InMontage,
	float InPlayRate,
	float InStartTimeSeconds,
	FName InStartSection)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return INDEX_NONE;
	if (!InMontage) return INDEX_NONE;

	int32 StartFrame = CachedFinalizedSimFrame;

	if (StartFrame == INDEX_NONE && CachedMoverComponent)
	{
		StartFrame = CachedMoverComponent->GetLastTimeStep().ServerFrame;
	}

	RepMontageState.Montage = InMontage;
	RepMontageState.PlayRate = InPlayRate;
	RepMontageState.StartMontageTimeSeconds = InStartTimeSeconds;
	RepMontageState.StartSection = InStartSection;
	RepMontageState.StartSimFrame = StartFrame;
	RepMontageState.Serial++;
	RepMontageState.bIsPlaying = true;

	return RepMontageState.Serial;
}

void UPL_MontageReplicationComponent::StopReplicatedMontageIfCurrent(int32 ExpectedSerial)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (ExpectedSerial == INDEX_NONE) return;
	if (!RepMontageState.bIsPlaying) return;
	if (RepMontageState.Serial != ExpectedSerial) return;

	RepMontageState.StartSimFrame = INDEX_NONE;
	RepMontageState.Serial++;
	RepMontageState.bIsPlaying = false;

	ClearActiveMontagePolicy();
}

bool UPL_MontageReplicationComponent::CanStartMontageWithPolicy(const FPLMontagePlayPolicy& NewPolicy) const
{
	if (!RepMontageState.bIsPlaying || !bHasActiveMontagePolicy) return true;
	if (!NewPolicy.bCanInterruptOthers) return false;
	if (!ActiveMontagePolicy.bCanBeInterrupted) return false;

	const bool bSameChannel = !ActiveMontagePolicy.MontageChannel.IsValid() ||
		!NewPolicy.MontageChannel.IsValid() ||
		ActiveMontagePolicy.MontageChannel == NewPolicy.MontageChannel;

	if (!bSameChannel) return false;

	return NewPolicy.InterruptPriority >= ActiveMontagePolicy.InterruptPriority;
}

void UPL_MontageReplicationComponent::SetActiveMontagePolicy(const FPLMontagePlayPolicy& InPolicy)
{
	ActiveMontagePolicy = InPolicy;
	bHasActiveMontagePolicy = true;
}

void UPL_MontageReplicationComponent::ClearActiveMontagePolicy()
{
	ActiveMontagePolicy = FPLMontagePlayPolicy();
	bHasActiveMontagePolicy = false;
}

void UPL_MontageReplicationComponent::HandleMoverPostFinalize(const FMoverSyncState& SyncState,
	const FMoverAuxStateContext& AuxState)
{
	if (!CachedMoverComponent) return;

	const FMoverTimeStep& TimeStep = CachedMoverComponent->GetLastTimeStep();
	CachedFinalizedSimFrame = TimeStep.ServerFrame;
}

void UPL_MontageReplicationComponent::BindToMoverPostFinalize()
{
	if (CachedMoverComponent) return;

	CachedMoverComponent = GetOwner() ? GetOwner()->FindComponentByClass<UMoverComponent>() : nullptr;
	if (!CachedMoverComponent) return;

	CachedMoverComponent->OnPostFinalize.AddDynamic(this, &UPL_MontageReplicationComponent::HandleMoverPostFinalize);
}

void UPL_MontageReplicationComponent::UnbindFromMoverPostFinalize()
{
	if (!CachedMoverComponent) return;

	CachedMoverComponent->OnPostFinalize.RemoveDynamic(this, &UPL_MontageReplicationComponent::HandleMoverPostFinalize);
	CachedMoverComponent = nullptr;
}

void UPL_MontageReplicationComponent::OnRep_RepMontageState()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;
	if (OwnerActor->GetLocalRole() != ROLE_SimulatedProxy) return;

	USkeletalMeshComponent* MeshComponent = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!MeshComponent) return;

	UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();
	if (!AnimInstance) return;

	if (!RepMontageState.bIsPlaying)
	{
		if (RepMontageState.Montage && AnimInstance->Montage_IsPlaying(RepMontageState.Montage))
		{
			AnimInstance->Montage_Stop(RepMontageState.Montage->GetDefaultBlendOutTime(), RepMontageState.Montage);
		}
		return;
	}

	if (!RepMontageState.Montage) return;
	if (!CachedMoverComponent) return;
	if (RepMontageState.StartSimFrame == INDEX_NONE) return;

	const FMoverTimeStep& TimeStep = CachedMoverComponent->GetLastTimeStep();

	const int32 CurrentSimFrame = CachedFinalizedSimFrame != INDEX_NONE
		? CachedFinalizedSimFrame
		: TimeStep.ServerFrame;

	if (CurrentSimFrame == INDEX_NONE) return;

	const int32 FramesSinceStart = FMath::Max(0, CurrentSimFrame - RepMontageState.StartSimFrame);
	const float StepSeconds = TimeStep.StepMs * 0.001f;

	float MontagePosition = RepMontageState.StartMontageTimeSeconds;
	MontagePosition += FramesSinceStart * StepSeconds * RepMontageState.PlayRate;
	MontagePosition = FMath::Clamp(MontagePosition, 0.f, RepMontageState.Montage->GetPlayLength());

	const float PlayedLength = AnimInstance->Montage_Play(RepMontageState.Montage, RepMontageState.PlayRate,
		EMontagePlayReturnType::MontageLength, MontagePosition, false);

	if (PlayedLength <= 0.f) return;

	if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(RepMontageState.Montage))
	{
		MontageInstance->PushDisableRootMotion();
	}
}
