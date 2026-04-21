#include "Pawn/PL_BasePawn.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet.h"
#include "Combat/Components/PL_CombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GAS/Attribute/PL_AttributeSet.h"
#include "GameFramework/PlayerState.h"
#include "Pawn/PL_NPCPawn.h"
#include "Player/PL_PlayerState.h"

APL_BasePawn::APL_BasePawn()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(true);
	// SetReplicatingMovement(false);

	CharacterMoverComponent = CreateDefaultSubobject<UCharacterMoverComponent>(TEXT("CharacterMoverComponent"));
	CombatComponent = CreateDefaultSubobject<UPL_CombatComponent>(TEXT("CombatComponent"));

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	SetRootComponent(CapsuleComponent.Get());
	CapsuleComponent->InitCapsuleSize(34.0f, 88.0f);
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CapsuleComponent->SetCollisionObjectType(ECC_Pawn);
	CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Block);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CapsuleComponent.Get());
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	MeshComponent->SetRelativeRotation(FRotator(0.0f, 0.0f, -90.0f));
}

UAbilitySystemComponent* APL_BasePawn::GetAbilitySystemComponent() const
{
	if (AbilitySystemComponent)
	{
		return AbilitySystemComponent;
	}

	if (const APlayerState* CurrentPlayerState = GetPlayerState())
	{
		if (const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(CurrentPlayerState))
		{
			return AbilitySystemInterface->GetAbilitySystemComponent();
		}
	}

	if (const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		return AbilitySystemInterface->GetAbilitySystemComponent();
	}

	return FindComponentByClass<UAbilitySystemComponent>();
}

UAttributeSet* APL_BasePawn::GetAttributeSet() const
{
	if (AttributeSet)
	{
		return AttributeSet;
	}

	if (const APL_PlayerState* PLPlayerState = GetPlayerState<APL_PlayerState>())
	{
		return PLPlayerState->GetAttributeSet();
	}

	if (const APL_NPCPawn* NPCPawn = Cast<APL_NPCPawn>(this))
	{
		return NPCPawn->GetOwnedAttributeSet();
	}
	
	return nullptr;
}

void APL_BasePawn::BeginPlay()
{
	Super::BeginPlay();

	ApplyConfiguredMovementSettings();
}

void APL_BasePawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (!CharacterMoverComponent) return;

	if (CapsuleComponent)
	{
		CharacterMoverComponent->SetUpdatedComponent(CapsuleComponent.Get());
	}

	if (MeshComponent)
	{
		CharacterMoverComponent->SetPrimaryVisualComponent(MeshComponent.Get());

		// Set this in editor if you prefer, but the idea is:
		// Smoothing Mode = Visual Component Offset
	}
}

void APL_BasePawn::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	FCharacterDefaultInputs& CharacterInputs =
		InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	CharacterInputs.OrientationIntent = FVector::ZeroVector;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, CachedMoveInputIntent);
		return;
	}

	const FRotator ControlRotation = PC->GetControlRotation();
	const FRotator YawRotation(0.f, ControlRotation.Yaw, 0.f);
	const FVector WorldMoveIntent = YawRotation.RotateVector(CachedMoveInputIntent);

	CharacterInputs.ControlRotation = ControlRotation;
	CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, WorldMoveIntent);

	// Strafe:
	// move based on camera yaw, but face camera yaw only while actually moving
	if (WorldMoveIntent.SizeSquared2D() > 0.01f)
	{
		CharacterInputs.OrientationIntent = YawRotation.Vector();
	}
}

void APL_BasePawn::RequestMoveIntent(const FVector& MoveIntent)
{
	CachedMoveInputIntent = MoveIntent;
}

void APL_BasePawn::ClearMoveIntent()
{
	CachedMoveInputIntent = FVector::ZeroVector;
}

void APL_BasePawn::InitializeAbilitySystemReferences(AActor* OwnerActor,
	UAbilitySystemComponent* InAbilitySystemComponent, UAttributeSet* InAttributeSet)
{
	AbilitySystemComponent = InAbilitySystemComponent;
	AttributeSet = InAttributeSet;

	if (AbilitySystemComponent && OwnerActor)
	{
		AbilitySystemComponent->InitAbilityActorInfo(OwnerActor, this);
	}

	if (CombatComponent && AbilitySystemComponent)
	{
		CombatComponent->InitializeCombat(this, AbilitySystemComponent);
	}
}

void APL_BasePawn::ClearAbilitySystemReferences()
{
	if (CombatComponent)
	{
		CombatComponent->DeinitializeCombat();
	}

	AbilitySystemComponent = nullptr;
	AttributeSet = nullptr;
}

void APL_BasePawn::ApplyConfiguredMovementSettings()
{
	if (!MovementSettings || !CharacterMoverComponent) return;

	UCommonLegacyMovementSettings* RuntimeMovementSettings =
		CharacterMoverComponent->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();

	if (!RuntimeMovementSettings) return;
	
	RuntimeMovementSettings->MaxSpeed = 400.f;
	RuntimeMovementSettings->Acceleration = 4000.f;
	RuntimeMovementSettings->Deceleration = 4000.f;
	RuntimeMovementSettings->TurningRate = 720.f;
	RuntimeMovementSettings->TurningBoost = 2.f;
}
