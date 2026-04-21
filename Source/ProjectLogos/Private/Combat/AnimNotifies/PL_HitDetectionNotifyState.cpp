#include "Combat/AnimNotifies/PL_HitDetectionNotifyState.h"
#include "Combat/Components/PL_CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	UPL_CombatComponent* ResolveCombatComponent(USkeletalMeshComponent* MeshComp)
	{
		if (!MeshComp) return nullptr;

		AActor* OwnerActor = MeshComp->GetOwner();
		if (!OwnerActor) return nullptr;

		return OwnerActor->FindComponentByClass<UPL_CombatComponent>();
	}
}

void UPL_HitDetectionNotifyState::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	UPL_CombatComponent* CombatComponent = ResolveCombatComponent(MeshComp);
	if (!CombatComponent) return;

	CombatComponent->BeginHitDetectionWindow(
		this,
		MeshComp,
		TraceSocketName,
		HitWindowSettings);
}

void UPL_HitDetectionNotifyState::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	UPL_CombatComponent* CombatComponent = ResolveCombatComponent(MeshComp);
	if (!CombatComponent) return;

	CombatComponent->EndHitDetectionWindow(this, MeshComp);
}
