// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "Components/SceneCaptureComponent2D.h"

#include "MyPawn.generated.h"

UCLASS()
class UENEURALNETWORK_API AMyPawn : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AMyPawn();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Handle move input
	void Move(const FInputActionValue& Value);

	// Handle look input
	void Look(const FInputActionValue& Value);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input")
		class UInputMappingContext* InputMapping;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input")
		class UMyInputConfigData* InputActions;

	UPROPERTY()
		class UFloatingPawnMovement* OurMovementComponent;

	UPROPERTY(EditAnywhere)
		class USpringArmComponent* SpringArmComp;

	UPROPERTY(EditAnywhere)
		class UCameraComponent* CameraComp;

	UPROPERTY(EditAnywhere)
		UStaticMeshComponent* StaticMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Camera)
		class USceneCaptureComponent2D* MySceneCapture;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Camera, meta = (AllowPrivateAccess = "true"))
		class UCaptureManager* MyCaptureManager;

};
