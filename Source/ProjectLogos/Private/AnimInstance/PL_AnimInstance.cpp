#include "AnimInstance/PL_AnimInstance.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"


void UPL_AnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	PawnOwner = TryGetPawnOwner();
	if (!PawnOwner) return;

	CharacterMoverComponent = PawnOwner->FindComponentByClass<UCharacterMoverComponent>();
}

void UPL_AnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!PawnOwner)
	{
		PawnOwner = TryGetPawnOwner();
		if (PawnOwner && !CharacterMoverComponent)
		{
			CharacterMoverComponent = PawnOwner->FindComponentByClass<UCharacterMoverComponent>();
		}
	}

	if (!PawnOwner || !CharacterMoverComponent) return;

	const FVector Velocity = CharacterMoverComponent->GetVelocity();

	GroundSpeed = Velocity.Size2D();
	AimRotation = PawnOwner->GetActorRotation();
	bIsInAir = IsInAir();

	float TargetOffsetYaw = 0.f;

	if (Velocity.SizeSquared2D() > 25.f)
	{
		MovementRotation = Velocity.GetSafeNormal2D().Rotation();
		TargetOffsetYaw = FMath::FindDeltaAngleDegrees(AimRotation.Yaw, MovementRotation.Yaw);
	}

	SmoothedMovementOffsetYaw = FMath::FInterpTo(
		SmoothedMovementOffsetYaw,
		TargetOffsetYaw,
		DeltaSeconds,
		10.f
	);

	MovementOffsetYaw = SmoothedMovementOffsetYaw;
}

bool UPL_AnimInstance::IsInAir() const
{
	if (!CharacterMoverComponent) return false;

	return CharacterMoverComponent->HasGameplayTag(
		FGameplayTag::RequestGameplayTag(TEXT("Mover.IsInAir")), true);
}
