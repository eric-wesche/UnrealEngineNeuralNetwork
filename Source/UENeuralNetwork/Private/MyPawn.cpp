// Fill out your copyright notice in the Description page of Project Settings.


#include "MyPawn.h"

#include "Camera/CameraComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "MyInputConfigData.h"

#include "Components/SphereComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/FloatingPawnMovement.h"

#include "CaptureManager.h"


// Sets default values
AMyPawn::AMyPawn()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// use controller for rotation
	bUseControllerRotationPitch = true;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = true;

	// root spehere component
	USphereComponent* SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("RootComponent"));
	RootComponent = SphereComponent;
	SphereComponent->InitSphereRadius(40.0f);
	SphereComponent->SetCollisionProfileName(TEXT("Pawn"));

	// mesh
	StaticMeshComp = CreateDefaultSubobject <UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereVisualAsset(TEXT("/Game/StarterContent/Shapes/Shape_Sphere.Shape_Sphere"));
	if (SphereVisualAsset.Succeeded())
	{
		StaticMeshComp->SetStaticMesh(SphereVisualAsset.Object);
		//StaticMeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, -40.0f));
		StaticMeshComp->SetWorldScale3D(FVector(0.8f));
	}

	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));

	// Attach our components
	StaticMeshComp->SetupAttachment(RootComponent);
	SpringArmComp->SetupAttachment(StaticMeshComp);
	CameraComp->SetupAttachment(SpringArmComp, USpringArmComponent::SocketName);
	CameraComp->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Use a spring arm to give the camera smooth, natural-feeling motion.
	SpringArmComp->SetupAttachment(RootComponent);
	SpringArmComp->SetRelativeRotation(FRotator(-45.f, 0.f, 0.f));
	SpringArmComp->TargetArmLength = 400.0f;

	// create scene capture component and attach to mesh
	MySceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("MySceneCapture"));
	MySceneCapture->SetupAttachment(StaticMeshComp);
	//MySceneCapture->SetRelativeRotation(FRotator(0.0f, 90.0f, -90.0f));
	//MySceneCapture->SetRelativeLocation(FVector(10.0f, 10.0f, 0.0f));

	//init capture manager
	MyCaptureManager = CreateDefaultSubobject<UCaptureManager>(TEXT("MyCaptureManager"));

	check(MyCaptureManager);
	MyCaptureManager->ColorCaptureComponents = MySceneCapture;

	//get neural network asset from content browser
	UNeuralNetwork* NeuralNetwork = Cast<UNeuralNetwork>(StaticLoadObject(UNeuralNetwork::StaticClass(), NULL,
		TEXT("/Script/NeuralNetworkInference.NeuralNetwork'/Game/Models/yolov8n_16_640_480.yolov8n_16_640_480'")));
	NeuralNetwork->SetSynchronousMode(ENeuralSynchronousMode::Synchronous);
	// set neural network
	MyCaptureManager->setNeuralNetwork(NeuralNetwork);

	// Set this pawn to be controlled by the lowest-numbered player when game starts.
	AutoPossessPlayer = EAutoReceiveInput::Player0;

	// Create an instance of our movement component, and tell it to update the root.
	OurMovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("CustomMovementComponent"));
	OurMovementComponent->UpdatedComponent = RootComponent;

}

// Called when the game starts or when spawned
void AMyPawn::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AMyPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AMyPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Get the player controller
	APlayerController* PC = Cast<APlayerController>(GetController());

	// Get the local player subsystem
	UEnhancedInputLocalPlayerSubsystem* Subsystem = 
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	// Clear out existing mapping, and add our mapping
	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(InputMapping, 0);

	// Get the EnhancedInputComponent
	UEnhancedInputComponent* PEI = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	// Bind the actions
	PEI->BindAction(InputActions->InputMove, ETriggerEvent::Triggered, this, &AMyPawn::Move);
	PEI->BindAction(InputActions->InputLook, ETriggerEvent::Triggered, this, &AMyPawn::Look);
	
}

void AMyPawn::Move(const FInputActionValue& Value)
{
	if (OurMovementComponent && (OurMovementComponent->UpdatedComponent == RootComponent))
	{
		const FVector2D MoveValue = Value.Get<FVector2D>();
		const FRotator MovementRotation(Controller->GetControlRotation().Pitch, Controller->GetControlRotation().Yaw, 0);

		// Forward/Backward direction
		if (MoveValue.Y != 0.f)
		{
			// Get forward vector
			const FVector Direction = MovementRotation.RotateVector(FVector::ForwardVector);
			AddMovementInput(Direction, MoveValue.Y);
		}

		// Right/Left direction
		if (MoveValue.X != 0.f)
		{
			// Get right vector
			const FVector Direction = MovementRotation.RotateVector(FVector::RightVector);
			AddMovementInput(Direction, MoveValue.X);
		}
	}
}

void AMyPawn::Look(const FInputActionValue& Value)
{
	if (Controller != nullptr)
	{
		const FVector2D LookValue = Value.Get<FVector2D>();

		if (LookValue.X != 0.f)
		{
			AddControllerYawInput(LookValue.X);
		}

		if (LookValue.Y != 0.f)
		{
			AddControllerPitchInput(LookValue.Y);
		}
	}
}



