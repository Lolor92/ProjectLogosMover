#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PL_MontagePlayPolicy.generated.h"

USTRUCT(BlueprintType)
struct FPLMontagePlayPolicy
{
	GENERATED_BODY()

	// Can this montage be interrupted by another one?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Montage Policy")
	bool bCanBeInterrupted = true;

	// Is this montage allowed to interrupt something else?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Montage Policy")
	bool bCanInterruptOthers = true;

	// Higher number wins when priorities are compared.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Montage Policy")
	int32 InterruptPriority = 0;

	// Montages in different channels usually should not fight each other.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Montage Policy")
	FGameplayTag MontageChannel;
};
