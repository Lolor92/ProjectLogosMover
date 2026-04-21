// Copyright ProjectLogos

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PL_AnimInstance.generated.h"

class UCharacterMoverComponent;

UCLASS()
class PROJECTLOGOS_API UPL_AnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// Anim instance lifecycle.
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	
	bool IsInAir() const;

private:
	// Movement values for the AnimBP.
	UPROPERTY(BlueprintReadOnly, Category="Anim|Movement", meta=(AllowPrivateAccess="true"))
	float GroundSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Anim|Movement", meta=(AllowPrivateAccess="true"))
	bool bIsAccelerating = false;

	UPROPERTY(BlueprintReadOnly, Category="Anim|Movement", meta=(AllowPrivateAccess="true"))
	bool bIsOnGround = false;
	
	UPROPERTY(BlueprintReadOnly, Category="Anim|Movement", meta=(AllowPrivateAccess="true"))
	bool bIsInAir = false;

	// Rotation values for the AnimBP.
	UPROPERTY(BlueprintReadOnly, Category="Anim|Movement", meta=(AllowPrivateAccess="true"))
	FRotator AimRotation;

	UPROPERTY(BlueprintReadOnly, Category="Anim|Movement", meta=(AllowPrivateAccess="true"))
	FRotator MovementRotation;

	UPROPERTY(BlueprintReadOnly, Category="Anim|Movement", meta=(AllowPrivateAccess="true"))
	float MovementOffsetYaw = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category="Anim|Movement", meta=(AllowPrivateAccess="true"))
	float SmoothedMovementOffsetYaw = 0.f;
	
	UPROPERTY()
	UCharacterMoverComponent* CharacterMoverComponent = nullptr;
	
	UPROPERTY()
	APawn* PawnOwner = nullptr;
};
