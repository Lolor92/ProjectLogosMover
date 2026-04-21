#include "Combat/Components/PL_CombatComponent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Pawn/PL_BasePawn.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffect.h"
#include "MoveLibrary/PlayMoverMontageCallbackProxy.h"
#include "MoverComponent.h"
#include "Animation/AnimInstance.h"
#include "Combat//Components/Runtime/PL_LocalPredictedReactionRuntime.h"
#include "MoveLibrary/PlayMoverMontageCallbackProxy.h"
#include "MoverComponent.h"

UPL_CombatComponent::UPL_CombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UPL_CombatComponent::BeginPlay()
{
	Super::BeginPlay();
	
	if (!OwningPawn)
	{
		OwningPawn = Cast<APL_BasePawn>(GetOwner());
	}
}

void UPL_CombatComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!ActiveHitWindow.bIsActive) return;

	TickHitDetectionWindow();
}

void UPL_CombatComponent::InitializeCombat(APL_BasePawn* InPawn, UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (AbilitySystemComponent && AbilitySystemComponent != InAbilitySystemComponent)
	{
		DeinitializeCombat();
	}

	OwningPawn = InPawn;
	AbilitySystemComponent = InAbilitySystemComponent;

	GrantDefaultAbilities();
}

void UPL_CombatComponent::DeinitializeCombat()
{
	ClearDefaultAbilities();

	ActiveHitWindow.Reset();

	OwningPawn = nullptr;
	AbilitySystemComponent = nullptr;
}

void UPL_CombatComponent::GrantDefaultAbilities()
{
	if (!AbilitySystemComponent || !OwningPawn || !OwningPawn->HasAuthority())
	{
		return;
	}

	for (const UPL_AbilitySet* AbilitySet : DefaultAbilitySets)
	{
		if (!AbilitySet) continue;

		AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &DefaultAbilityHandles, OwningPawn);
	}
}

void UPL_CombatComponent::ClearDefaultAbilities()
{
	if (!AbilitySystemComponent || !OwningPawn || !OwningPawn->HasAuthority())
	{
		return;
	}

	DefaultAbilityHandles.TakeFromAbilitySystem(AbilitySystemComponent);
}

bool UPL_CombatComponent::BeginHitDetectionWindow(
	const UAnimNotifyState* NotifyState,
	USkeletalMeshComponent* MeshComp,
	FName TraceSocketName,
	const FPLHitWindowSettings& HitWindowSettings)
{
	if (!NotifyState || !MeshComp) return false;

	ActiveHitWindow.MeshComponent = MeshComp;
	ActiveHitWindow.NotifyState = NotifyState;
	ActiveHitWindow.TraceSocketName = TraceSocketName;
	ActiveHitWindow.HitWindowSettings = HitWindowSettings;
	ActiveHitWindow.HitActorsThisWindow.Reset();
	ActiveHitWindow.bIsActive = true;

	return true;
}

void UPL_CombatComponent::EndHitDetectionWindow(
	const UAnimNotifyState* NotifyState,
	USkeletalMeshComponent* MeshComp)
{
	if (!ActiveHitWindow.bIsActive) return;
	if (ActiveHitWindow.NotifyState != NotifyState) return;
	if (ActiveHitWindow.MeshComponent != MeshComp) return;

	ActiveHitWindow.Reset();
}

FTransform UPL_CombatComponent::GetHitTraceWorldTransform() const
{
	if (!ActiveHitWindow.MeshComponent) return FTransform::Identity;

	FTransform SocketTransform = ActiveHitWindow.TraceSocketName != NAME_None
		? ActiveHitWindow.MeshComponent->GetSocketTransform(ActiveHitWindow.TraceSocketName, RTS_World)
		: ActiveHitWindow.MeshComponent->GetComponentTransform();

	const FPLHitWindowShapeSettings& ShapeSettings = ActiveHitWindow.HitWindowSettings.ShapeSettings;
	const FTransform LocalOffsetTransform(ShapeSettings.LocalRotation, ShapeSettings.LocalOffset);

	return LocalOffsetTransform * SocketTransform;
}

void UPL_CombatComponent::TickHitDetectionWindow()
{
	const bool bIsAuthority = OwningPawn && OwningPawn->HasAuthority();
	const bool bIsLocalPrediction = OwningPawn && OwningPawn->IsLocallyControlled() && !bIsAuthority;

	if (!bIsAuthority && !bIsLocalPrediction) return;
	if (!GetWorld()) return;
	if (!ActiveHitWindow.MeshComponent) return;

	const FTransform TraceTransform = GetHitTraceWorldTransform();
	const FVector TraceLocation = TraceTransform.GetLocation();
	const FQuat TraceRotation = TraceTransform.GetRotation();

	const FPLHitWindowShapeSettings& ShapeSettings = ActiveHitWindow.HitWindowSettings.ShapeSettings;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PL_HitWindowTrace), false, GetOwner());
	QueryParams.AddIgnoredActor(GetOwner());

	TArray<FOverlapResult> OverlapResults;
	FCollisionShape CollisionShape;

	switch (ShapeSettings.ShapeType)
	{
	case EPLHitDetectionShapeType::Sphere:
		CollisionShape = FCollisionShape::MakeSphere(ShapeSettings.SphereRadius);
		break;

	case EPLHitDetectionShapeType::Capsule:
		CollisionShape = FCollisionShape::MakeCapsule(
			ShapeSettings.CapsuleRadius,
			ShapeSettings.CapsuleHalfHeight);
		break;

	case EPLHitDetectionShapeType::Box:
		CollisionShape = FCollisionShape::MakeBox(ShapeSettings.BoxHalfExtent);
		break;

	default:
		return;
	}

	const bool bHasOverlap = GetWorld()->OverlapMultiByObjectType(
		OverlapResults,
		TraceLocation,
		TraceRotation,
		ObjectQueryParams,
		CollisionShape,
		QueryParams);

	if (ActiveHitWindow.HitWindowSettings.DebugSettings.bDrawDebugTrace)
	{
		switch (ShapeSettings.ShapeType)
		{
		case EPLHitDetectionShapeType::Sphere:
			DrawDebugSphere(GetWorld(), TraceLocation, ShapeSettings.SphereRadius, 16, FColor::Red, false, 0.05f);
			break;

		case EPLHitDetectionShapeType::Capsule:
			DrawDebugCapsule(
				GetWorld(),
				TraceLocation,
				ShapeSettings.CapsuleHalfHeight,
				ShapeSettings.CapsuleRadius,
				TraceRotation,
				FColor::Red,
				false,
				0.05f);
			break;

		case EPLHitDetectionShapeType::Box:
			DrawDebugBox(
				GetWorld(),
				TraceLocation,
				ShapeSettings.BoxHalfExtent,
				TraceRotation,
				FColor::Red,
				false,
				0.05f);
			break;

		default:
			break;
		}
	}

	if (!bHasOverlap) return;

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* HitActor = OverlapResult.GetActor();
		if (!HitActor || HitActor == GetOwner()) continue;

		TWeakObjectPtr<AActor> WeakHitActor(HitActor);
		if (ActiveHitWindow.HitActorsThisWindow.Contains(WeakHitActor)) continue;

		ActiveHitWindow.HitActorsThisWindow.Add(WeakHitActor);

		if (bIsAuthority)
		{
			ApplyHitWindowEffectsToActor(HitActor);
		}

		if (bIsLocalPrediction)
		{
			TryStartPredictedMoverHitReaction(HitActor);
		}
	}
}

void UPL_CombatComponent::ApplyHitWindowEffectsToActor(AActor* HitActor)
{
	if (!HitActor) return;
	if (!AbilitySystemComponent) return;

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!TargetASC) return;

	for (const FPLHitWindowGameplayEffect& EffectEntry : ActiveHitWindow.HitWindowSettings.GameplayEffectsToApply)
	{
		if (!EffectEntry.GameplayEffectClass) continue;

		FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(GetOwner());

		FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
			EffectEntry.GameplayEffectClass,
			EffectEntry.EffectLevel,
			EffectContext);

		if (!SpecHandle.IsValid()) continue;

		AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	}
}

void UPL_CombatComponent::PlayPredictedHitReaction(AActor* HitActor)
{
	if (!HitActor) return;
	if (!OwningPawn || !OwningPawn->IsLocallyControlled() || OwningPawn->HasAuthority()) return;

	UAnimMontage* MontageToPlay = ActiveHitWindow.HitWindowSettings.PredictedReactionMontage;
	if (!MontageToPlay) return;

	USkeletalMeshComponent* TargetMesh = HitActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!TargetMesh) return;

	UAnimInstance* TargetAnimInstance = TargetMesh->GetAnimInstance();
	if (!TargetAnimInstance) return;

	const float PlayedLength = TargetAnimInstance->Montage_Play(
		MontageToPlay,
		1.f,
		EMontagePlayReturnType::MontageLength,
		0.f,
		false);

	if (PlayedLength <= 0.f) return;

	if (FAnimMontageInstance* MontageInstance = TargetAnimInstance->GetActiveInstanceForMontage(MontageToPlay))
	{
		MontageInstance->PushDisableRootMotion();
	}
}

bool UPL_CombatComponent::DebugPlayPredictedMoverReaction(AActor* HitActor, UAnimMontage* ReactionMontage)
{
	if (!OwningPawn || !OwningPawn->IsLocallyControlled() || OwningPawn->HasAuthority()) return false;
	if (!HitActor || !ReactionMontage) return false;

	UMoverComponent* TargetMover = HitActor->FindComponentByClass<UMoverComponent>();
	if (!TargetMover) return false;

	UPlayMoverMontageCallbackProxy* Proxy = UPlayMoverMontageCallbackProxy::CreateProxyObjectForPlayMoverMontage(
		TargetMover,
		ReactionMontage,
		1.f,
		0.f,
		NAME_None);

	return Proxy != nullptr;
}

bool UPL_CombatComponent::TryStartPredictedMoverHitReaction(AActor* HitActor)
{
	if (!OwningPawn || !OwningPawn->IsLocallyControlled() || OwningPawn->HasAuthority()) return false;
	if (!HitActor) return false;

	UAnimMontage* ReactionMontage = ActiveHitWindow.HitWindowSettings.PredictedReactionMontage;
	if (!ReactionMontage) return false;

	UMoverComponent* TargetMover = HitActor->FindComponentByClass<UMoverComponent>();
	if (!TargetMover) return false;

	USkeletalMeshComponent* TargetMesh = HitActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!TargetMesh) return false;

	UAnimInstance* TargetAnimInstance = TargetMesh->GetAnimInstance();
	if (!TargetAnimInstance) return false;

	UPlayMoverMontageCallbackProxy* Proxy = UPlayMoverMontageCallbackProxy::CreateProxyObjectForPlayMoverMontage(
		TargetMover,
		ReactionMontage,
		1.f,
		0.f,
		NAME_None);

	if (!Proxy) return false;
	if (!TargetAnimInstance->GetActiveInstanceForMontage(ReactionMontage)) return false;

	ActivePredictedReactionProxies.Add(Proxy);

	UMoverComponent* PredictionFrameMover = OwningPawn
	? OwningPawn->FindComponentByClass<UMoverComponent>()
	: nullptr;

	if (!PredictionFrameMover) return false;

	const int32 PredictedStartFrame = PredictionFrameMover->GetLastTimeStep().ServerFrame + 1;

	if (UPL_LocalPredictedReactionRuntime* Runtime = GetWorld()->GetSubsystem<UPL_LocalPredictedReactionRuntime>())
	{
		Runtime->RegisterPredictedReaction(
			HitActor,
			PredictionFrameMover,
			ReactionMontage,
			PredictedStartFrame,
			1.f,
			0.f);
	}

	return true;
}
