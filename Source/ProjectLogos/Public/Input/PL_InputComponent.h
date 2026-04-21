#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Input/PL_InputConfig.h"
#include "TimerManager.h"
#include "PL_InputComponent.generated.h"

class AController;
class APlayerController;
class APawn;
class UAbilitySystemComponent;
class UCameraComponent;
class UEnhancedInputComponent;
class UGameplayAbility;
class USpringArmComponent;
struct FGameplayAbilitySpec;
struct FInputActionValue;

struct FPLActiveComboChain
{
	FGameplayAbilitySpecHandle CurrentAbilityHandle;
	TSubclassOf<UGameplayAbility> NextAbilityClass = nullptr;
	FTimerHandle TimerHandle;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTLOGOS_API UPL_InputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPL_InputComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UPL_InputConfig> InputConfig = nullptr;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Zoom")
	bool bEnableZoom = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Zoom", meta = (ClampMin = "0"))
	int32 MinZoomLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Zoom", meta = (ClampMin = "0"))
	int32 MaxZoomLevel = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Zoom", meta = (ClampMin = "0"))
	int32 ZoomLevel = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Zoom", meta = (ClampMin = "0.0"))
	float ZoomStepDistance = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Zoom", meta = (ClampMin = "0.0"))
	float ZoomInterpSpeed = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Zoom")
	FVector DefaultCameraOffset = FVector(0.f, 0.f, 10.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Zoom")
	FVector ClosestZoomCameraOffset = FVector(50.f, 0.f, 70.f);

private:
	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	void HandleNewPawn(APawn* NewPawn);
	void BindToPlayerController(APlayerController* PlayerController);
	void UnbindFromPlayerController();
	void InstallForPawn(APawn* Pawn);
	void UninstallFromPawn();

	void AddMappingContextsForLocalPlayer() const;
	void RemoveMappingContextsForLocalPlayer() const;
	void BindActionsFromConfig();

	bool IsLocallyControlledOwner() const;
	APlayerController* GetOwningPlayerController() const;
	UAbilitySystemComponent* GetAbilitySystemComponent() const;

	void HandleActionPressed(FGameplayTag InputTag);
	void HandleActionReleased(FGameplayTag InputTag);

	bool TryActivateComboAbility(const FGameplayAbilitySpec& RequestedAbilitySpec);
	void UpdateComboChain(FGameplayAbilitySpecHandle StarterHandle, const FGameplayAbilitySpec& CurrentAbilitySpec);
	void ClearComboChain(FGameplayAbilitySpecHandle StarterHandle);
	void ClearAllComboChains();

	void Move(const FInputActionValue& Value);
	void StopMove(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Zoom(const FInputActionValue& Value);
	void ApplyZoom();

	USpringArmComponent* FindSpringArm() const;
	UCameraComponent* FindCamera() const;

	TMap<FGameplayAbilitySpecHandle, FPLActiveComboChain> ActiveComboChains;

	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputComponent> InjectedEnhancedInputComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CachedSpringArm = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> CachedCamera = nullptr;

	UPROPERTY(Transient)
	float DesiredZoomArmLength = 0.f;

	FDelegateHandle NewPawnHandle;
	TWeakObjectPtr<APlayerController> CachedPlayerController;
};
