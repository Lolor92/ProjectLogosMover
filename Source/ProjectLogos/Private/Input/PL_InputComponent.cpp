#include "Input/PL_InputComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayAbilitySpec.h"
#include "GAS/Ability/PL_GameplayAbility.h"
#include "Input/Tag/PL_InputTags.h"
#include "InputActionValue.h"
#include "Pawn/PL_BasePawn.h"

UPL_InputComponent::UPL_InputComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPL_InputComponent::BeginPlay()
{
	Super::BeginPlay();

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.AddDynamic(this, &ThisClass::HandleControllerChanged);
	}

	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		BindToPlayerController(PlayerController);

		APawn* InitialPawn = PlayerController->GetPawn();
		HandleNewPawn(InitialPawn ? InitialPawn : Cast<APawn>(GetOwner()));
	}
}

void UPL_InputComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &ThisClass::HandleControllerChanged);
	}

	UninstallFromPawn();
	UnbindFromPlayerController();

	Super::EndPlay(EndPlayReason);
}

void UPL_InputComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USpringArmComponent* SpringArm = FindSpringArm();
	if (!SpringArm)
	{
		SetComponentTickEnabled(false);
		return;
	}

	SpringArm->TargetArmLength = FMath::FInterpTo(
		SpringArm->TargetArmLength,
		DesiredZoomArmLength,
		DeltaTime,
		ZoomInterpSpeed);

	if (FMath::IsNearlyEqual(SpringArm->TargetArmLength, DesiredZoomArmLength, 1.f))
	{
		SpringArm->TargetArmLength = DesiredZoomArmLength;
		SetComponentTickEnabled(false);
	}
}

void UPL_InputComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	if (Pawn != GetOwner())
	{
		return;
	}

	UninstallFromPawn();
	UnbindFromPlayerController();

	APlayerController* PlayerController = Cast<APlayerController>(NewController);
	if (!PlayerController)
	{
		return;
	}

	BindToPlayerController(PlayerController);

	APawn* PossessedPawn = PlayerController->GetPawn();
	HandleNewPawn(PossessedPawn ? PossessedPawn : Pawn);
}

void UPL_InputComponent::HandleNewPawn(APawn* NewPawn)
{
	UninstallFromPawn();

	if (!NewPawn || NewPawn != GetOwner() || !IsLocallyControlledOwner())
	{
		return;
	}

	InstallForPawn(NewPawn);
}

void UPL_InputComponent::BindToPlayerController(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}

	if (CachedPlayerController.Get() == PlayerController && NewPawnHandle.IsValid())
	{
		return;
	}

	UnbindFromPlayerController();

	CachedPlayerController = PlayerController;
	NewPawnHandle = PlayerController->GetOnNewPawnNotifier().AddUObject(this, &ThisClass::HandleNewPawn);
}

void UPL_InputComponent::UnbindFromPlayerController()
{
	if (APlayerController* PlayerController = CachedPlayerController.Get())
	{
		if (NewPawnHandle.IsValid())
		{
			PlayerController->GetOnNewPawnNotifier().Remove(NewPawnHandle);
		}
	}

	NewPawnHandle.Reset();
	CachedPlayerController = nullptr;
}

void UPL_InputComponent::InstallForPawn(APawn* Pawn)
{
	CachedPlayerController = GetOwningPlayerController();
	if (!CachedPlayerController.IsValid() || !CachedPlayerController->IsLocalController() || Pawn != GetOwner())
	{
		return;
	}

	CachedSpringArm = Pawn->FindComponentByClass<USpringArmComponent>();
	CachedCamera = Pawn->FindComponentByClass<UCameraComponent>();

	if (CachedSpringArm)
	{
		DesiredZoomArmLength = CachedSpringArm->TargetArmLength;
	}

	AddMappingContextsForLocalPlayer();

	if (APlayerController* PlayerController = CachedPlayerController.Get())
	{
		if (!InjectedEnhancedInputComponent)
		{
			InjectedEnhancedInputComponent = NewObject<UEnhancedInputComponent>(
				PlayerController,
				UEnhancedInputComponent::StaticClass(),
				TEXT("PL_InjectedInput"));
			InjectedEnhancedInputComponent->RegisterComponent();
			PlayerController->PushInputComponent(InjectedEnhancedInputComponent);
		}
	}

	BindActionsFromConfig();
}

void UPL_InputComponent::UninstallFromPawn()
{
	RemoveMappingContextsForLocalPlayer();
	ClearAllComboChains();

	if (APL_BasePawn* BasePawn = Cast<APL_BasePawn>(GetOwner()))
	{
		BasePawn->ClearMoveIntent();
	}

	if (APlayerController* PlayerController = CachedPlayerController.Get())
	{
		if (InjectedEnhancedInputComponent)
		{
			PlayerController->PopInputComponent(InjectedEnhancedInputComponent);
			InjectedEnhancedInputComponent->DestroyComponent();
			InjectedEnhancedInputComponent = nullptr;
		}
	}

	AbilitySystemComponent = nullptr;
	CachedSpringArm = nullptr;
	CachedCamera = nullptr;
	DesiredZoomArmLength = 0.f;
	SetComponentTickEnabled(false);
}

void UPL_InputComponent::AddMappingContextsForLocalPlayer() const
{
	if (!InputConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("PL_Input: InputConfig is null on %s."), *GetNameSafe(GetOwner()));
		return;
	}

	const APlayerController* PlayerController = CachedPlayerController.IsValid()
		? CachedPlayerController.Get()
		: GetOwningPlayerController();
	if (!PlayerController)
	{
		return;
	}

	const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	if (!Subsystem)
	{
		return;
	}

	for (const FPLInputMappingContextEntry& MappingEntry : InputConfig->MappingContexts)
	{
		if (!MappingEntry.InputMappingContext)
		{
			continue;
		}

		Subsystem->AddMappingContext(MappingEntry.InputMappingContext, MappingEntry.Priority);
	}
}

void UPL_InputComponent::RemoveMappingContextsForLocalPlayer() const
{
	if (!InputConfig)
	{
		return;
	}

	const APlayerController* PlayerController = CachedPlayerController.IsValid()
		? CachedPlayerController.Get()
		: GetOwningPlayerController();
	if (!PlayerController)
	{
		return;
	}

	const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	if (!Subsystem)
	{
		return;
	}

	for (const FPLInputMappingContextEntry& MappingEntry : InputConfig->MappingContexts)
	{
		if (!MappingEntry.InputMappingContext)
		{
			continue;
		}

		Subsystem->RemoveMappingContext(MappingEntry.InputMappingContext);
	}
}

void UPL_InputComponent::BindActionsFromConfig()
{
	if (!InjectedEnhancedInputComponent || !InputConfig)
	{
		return;
	}

	for (const FPLInputAction& InputActionRow : InputConfig->InputActions)
	{
		if (!InputActionRow.InputAction || !InputActionRow.InputTag.IsValid())
		{
			continue;
		}

		if (InputActionRow.InputTag.MatchesTagExact(TAG_Input_Move))
		{
			InjectedEnhancedInputComponent->BindAction(
				InputActionRow.InputAction,
				ETriggerEvent::Triggered,
				this,
				&ThisClass::Move);
			InjectedEnhancedInputComponent->BindAction(
				InputActionRow.InputAction,
				ETriggerEvent::Completed,
				this,
				&ThisClass::StopMove);
			InjectedEnhancedInputComponent->BindAction(
				InputActionRow.InputAction,
				ETriggerEvent::Canceled,
				this,
				&ThisClass::StopMove);
			continue;
		}

		if (InputActionRow.InputTag.MatchesTagExact(TAG_Input_Look))
		{
			InjectedEnhancedInputComponent->BindAction(
				InputActionRow.InputAction,
				ETriggerEvent::Triggered,
				this,
				&ThisClass::Look);
			continue;
		}

		if (InputActionRow.InputTag.MatchesTagExact(TAG_Input_Zoom))
		{
			InjectedEnhancedInputComponent->BindAction(
				InputActionRow.InputAction,
				ETriggerEvent::Triggered,
				this,
				&ThisClass::Zoom);
			continue;
		}

		InjectedEnhancedInputComponent->BindAction(
			InputActionRow.InputAction,
			ETriggerEvent::Started,
			this,
			&ThisClass::HandleActionPressed,
			InputActionRow.InputTag);
		InjectedEnhancedInputComponent->BindAction(
			InputActionRow.InputAction,
			ETriggerEvent::Completed,
			this,
			&ThisClass::HandleActionReleased,
			InputActionRow.InputTag);
		InjectedEnhancedInputComponent->BindAction(
			InputActionRow.InputAction,
			ETriggerEvent::Canceled,
			this,
			&ThisClass::HandleActionReleased,
			InputActionRow.InputTag);
	}
}

bool UPL_InputComponent::IsLocallyControlledOwner() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const APlayerController* PlayerController = OwnerPawn
		? Cast<APlayerController>(OwnerPawn->GetController())
		: nullptr;

	return PlayerController && PlayerController->IsLocalController();
}

APlayerController* UPL_InputComponent::GetOwningPlayerController() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
}

UAbilitySystemComponent* UPL_InputComponent::GetAbilitySystemComponent() const
{
	if (AbilitySystemComponent)
	{
		return AbilitySystemComponent;
	}

	if (const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		return AbilitySystemInterface->GetAbilitySystemComponent();
	}

	return nullptr;
}

void UPL_InputComponent::HandleActionPressed(FGameplayTag InputTag)
{
	if (!AbilitySystemComponent)
	{
		AbilitySystemComponent = GetAbilitySystemComponent();
	}

	if (!AbilitySystemComponent)
	{
		return;
	}

	for (FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
	{
		const bool bMatchesInputTag = AbilitySpec.GetDynamicSpecSourceTags().HasTag(InputTag)
			|| (AbilitySpec.Ability && AbilitySpec.Ability->GetAssetTags().HasTag(InputTag));
		if (!bMatchesInputTag)
		{
			continue;
		}

		if (TryActivateComboAbility(AbilitySpec))
		{
			return;
		}

		const bool bActivated = AbilitySystemComponent->TryActivateAbility(AbilitySpec.Handle);
		if (bActivated)
		{
			UpdateComboChain(AbilitySpec.Handle, AbilitySpec);
		}

		AbilitySystemComponent->AbilitySpecInputPressed(AbilitySpec);

		FPredictionKey PredictionKey;
		if (UGameplayAbility* PrimaryInstance = AbilitySpec.GetPrimaryInstance())
		{
			PredictionKey = PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey();
		}

		AbilitySystemComponent->InvokeReplicatedEvent(
			EAbilityGenericReplicatedEvent::InputPressed,
			AbilitySpec.Handle,
			PredictionKey);
	}
}

void UPL_InputComponent::HandleActionReleased(FGameplayTag InputTag)
{
	if (!AbilitySystemComponent)
	{
		AbilitySystemComponent = GetAbilitySystemComponent();
	}

	if (!AbilitySystemComponent)
	{
		return;
	}

	for (FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
	{
		const bool bMatchesInputTag = AbilitySpec.GetDynamicSpecSourceTags().HasTag(InputTag)
			|| (AbilitySpec.Ability && AbilitySpec.Ability->GetAssetTags().HasTag(InputTag));
		if (!bMatchesInputTag || !AbilitySpec.IsActive())
		{
			continue;
		}

		AbilitySystemComponent->AbilitySpecInputReleased(AbilitySpec);

		FPredictionKey PredictionKey;
		if (UGameplayAbility* PrimaryInstance = AbilitySpec.GetPrimaryInstance())
		{
			PredictionKey = PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey();
		}

		AbilitySystemComponent->InvokeReplicatedEvent(
			EAbilityGenericReplicatedEvent::InputReleased,
			AbilitySpec.Handle,
			PredictionKey);
	}
}

bool UPL_InputComponent::TryActivateComboAbility(const FGameplayAbilitySpec& RequestedAbilitySpec)
{
	if (!AbilitySystemComponent)
	{
		return false;
	}

	FPLActiveComboChain* ComboChain = ActiveComboChains.Find(RequestedAbilitySpec.Handle);
	if (!ComboChain || !ComboChain->NextAbilityClass)
	{
		return false;
	}

	for (FGameplayAbilitySpec& ComboSpec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (!ComboSpec.Ability || !ComboSpec.Ability->GetClass()->IsChildOf(ComboChain->NextAbilityClass))
		{
			continue;
		}

		const bool bActivated = AbilitySystemComponent->TryActivateAbility(ComboSpec.Handle);
		if (bActivated)
		{
			UpdateComboChain(RequestedAbilitySpec.Handle, ComboSpec);
		}

		return true;
	}

	ClearComboChain(RequestedAbilitySpec.Handle);
	return true;
}

void UPL_InputComponent::UpdateComboChain(FGameplayAbilitySpecHandle StarterHandle,
	const FGameplayAbilitySpec& CurrentAbilitySpec)
{
	UPL_GameplayAbility* CurrentAbility = Cast<UPL_GameplayAbility>(CurrentAbilitySpec.GetPrimaryInstance());
	if (!CurrentAbility)
	{
		CurrentAbility = Cast<UPL_GameplayAbility>(CurrentAbilitySpec.Ability);
	}

	if (!CurrentAbility || !CurrentAbility->GetComboAbilityClass() || CurrentAbility->GetComboWindowDuration() <= 0.f)
	{
		ClearComboChain(StarterHandle);
		return;
	}

	FPLActiveComboChain& ComboChain = ActiveComboChains.FindOrAdd(StarterHandle);
	ComboChain.CurrentAbilityHandle = CurrentAbilitySpec.Handle;
	ComboChain.NextAbilityClass = CurrentAbility->GetComboAbilityClass();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ComboChain.TimerHandle);

		FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &ThisClass::ClearComboChain, StarterHandle);
		World->GetTimerManager().SetTimer(
			ComboChain.TimerHandle,
			TimerDelegate,
			CurrentAbility->GetComboWindowDuration(),
			false);
	}
}

void UPL_InputComponent::ClearComboChain(FGameplayAbilitySpecHandle StarterHandle)
{
	if (FPLActiveComboChain* ComboChain = ActiveComboChains.Find(StarterHandle))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ComboChain->TimerHandle);
		}
	}

	ActiveComboChains.Remove(StarterHandle);
}

void UPL_InputComponent::ClearAllComboChains()
{
	UWorld* World = GetWorld();
	for (TPair<FGameplayAbilitySpecHandle, FPLActiveComboChain>& ComboChainPair : ActiveComboChains)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(ComboChainPair.Value.TimerHandle);
		}
	}

	ActiveComboChains.Reset();
}

void UPL_InputComponent::Move(const FInputActionValue& Value)
{
	const FVector2D InputVector = Value.Get<FVector2D>();

	if (APL_BasePawn* BasePawn = Cast<APL_BasePawn>(GetOwner()))
	{
		BasePawn->RequestMoveIntent(FVector(InputVector.Y, InputVector.X, 0.f));
	}
}

void UPL_InputComponent::StopMove(const FInputActionValue& Value)
{
	if (APL_BasePawn* BasePawn = Cast<APL_BasePawn>(GetOwner()))
	{
		BasePawn->ClearMoveIntent();
	}
}

void UPL_InputComponent::Look(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();

	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController)
	{
		return;
	}

	PlayerController->AddYawInput(LookVector.X);
	PlayerController->AddPitchInput(LookVector.Y);
}

void UPL_InputComponent::Zoom(const FInputActionValue& Value)
{
	if (!bEnableZoom || MaxZoomLevel < MinZoomLevel)
	{
		return;
	}

	const float InputAxisValue = Value.Get<float>();
	if (FMath::IsNearlyZero(InputAxisValue))
	{
		return;
	}

	if (InputAxisValue > 0.f && ZoomLevel > MinZoomLevel)
	{
		--ZoomLevel;
		ApplyZoom();
	}
	else if (InputAxisValue < 0.f && ZoomLevel < MaxZoomLevel)
	{
		++ZoomLevel;
		ApplyZoom();
	}
}

void UPL_InputComponent::ApplyZoom()
{
	USpringArmComponent* SpringArm = FindSpringArm();
	if (!SpringArm)
	{
		return;
	}

	DesiredZoomArmLength = ZoomLevel * ZoomStepDistance;
	SetComponentTickEnabled(true);

	if (UCameraComponent* Camera = FindCamera())
	{
		const FVector& CameraOffset = ZoomLevel == MinZoomLevel ? ClosestZoomCameraOffset : DefaultCameraOffset;
		Camera->SetRelativeLocation(CameraOffset);
	}
}

USpringArmComponent* UPL_InputComponent::FindSpringArm() const
{
	if (CachedSpringArm)
	{
		return CachedSpringArm;
	}

	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Pawn->FindComponentByClass<USpringArmComponent>();
	}

	return nullptr;
}

UCameraComponent* UPL_InputComponent::FindCamera() const
{
	if (CachedCamera)
	{
		return CachedCamera;
	}

	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Pawn->FindComponentByClass<UCameraComponent>();
	}

	return nullptr;
}
