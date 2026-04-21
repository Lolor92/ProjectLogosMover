#include "Pawn/PL_PlayerPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/PL_InputComponent.h"
#include "Player/PL_PlayerState.h"


APL_PlayerPawn::APL_PlayerPawn()
{
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->SetupAttachment(MeshComponent);
	SpringArmComponent->TargetArmLength = 750.0f;
	SpringArmComponent->SetRelativeLocation(FVector(0.0f, 25.0f, 50.0f));
	SpringArmComponent->bUsePawnControlRotation = true;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent.Get());
	CameraComponent->bUsePawnControlRotation = false;

	PawnInputComponent = CreateDefaultSubobject<UPL_InputComponent>(TEXT("PawnInputComponent"));
}

void APL_PlayerPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitializeAbilitySystemFromPlayerState();
}

void APL_PlayerPawn::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializeAbilitySystemFromPlayerState();
}

void APL_PlayerPawn::InitializeAbilitySystemFromPlayerState()
{
	APL_PlayerState* PLPlayerState = GetPlayerState<APL_PlayerState>();
	if (!PLPlayerState)
	{
		if (APlayerState* CurrentPlayerState = GetPlayerState())
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("PL_PlayerPawn expected APL_PlayerState but got %s. Set your GameMode PlayerStateClass to APL_PlayerState."),
				*GetNameSafe(CurrentPlayerState->GetClass()));
		}

		ClearAbilitySystemReferences();
		return;
	}

	InitializeAbilitySystemReferences(PLPlayerState, PLPlayerState->GetAbilitySystemComponent(),
		PLPlayerState->GetAttributeSet());
}
