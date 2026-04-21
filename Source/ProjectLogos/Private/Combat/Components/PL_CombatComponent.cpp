#include "Combat/Components/PL_CombatComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Pawn/PL_BasePawn.h"

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
	if (!OwningPawn || !OwningPawn->HasAuthority()) return;
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
		ApplyHitWindowEffectsToActor(HitActor);
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
