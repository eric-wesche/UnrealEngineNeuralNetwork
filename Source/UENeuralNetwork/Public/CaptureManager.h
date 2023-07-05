// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CoreMinimal.h"

#include "Components/SceneCaptureComponent2D.h"

#include "NeuralNetwork.h"
#include "MyNeuralNetwork.h"

#include "Components/ActorComponent.h"

#include "Materials/MaterialInstanceDynamic.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Engine/CanvasRenderTarget2D.h"

#include "CaptureManager.generated.h"

// if declare this class below a place where it is used in initialization, it will cause compile error. so forward declare it here.
class AsyncInferenceTask;


USTRUCT()
struct FRenderRequest {
	GENERATED_BODY()

	TArray<FColor> Image;
	FRenderCommandFence RenderFence;
	bool isPNG;

	FRenderRequest() {
		isPNG = false;
	}
};

USTRUCT()
struct FScreenImageProperties {
	GENERATED_BODY()

	int32 width;
	int32 height;
};

USTRUCT()
struct FModelImageProperties {
	GENERATED_BODY()

	int32 width;
	int32 height;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UENEURALNETWORK_API UCaptureManager : public UActorComponent
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this component's properties
	UCaptureManager();

	// Color Capture  Components
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
		USceneCaptureComponent2D* ColorCaptureComponents;

	
	// Dynamic Material instance used to display on the hud
	static UMaterialInstanceDynamic* DynamicMaterialInstance;

	UFUNCTION(BlueprintPure)
	UMaterialInstanceDynamic* GetDynamicMaterialInstance()
	{
		return DynamicMaterialInstance;
	}
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
		UTextureRenderTarget2D* RenderTarget2D;
	
	static UCanvasRenderTarget2D* BoundingBoxRenderTarget2D;
	static UMyNeuralNetwork::FBoxCoordinates BoundingBoxCoordinates;

	UFUNCTION()
		void OnCanvasRenderTargetUpdate2(UCanvas* Canvas, int32 Width, int32 Height);
private:
	// RenderRequest Queue
	TQueue<FRenderRequest*> RenderRequestQueue;
	// inference task queue
	TQueue<FAsyncTask<AsyncInferenceTask>*> InferenceTaskQueue;
	// current inference task
	FAsyncTask<AsyncInferenceTask>* CurrentInferenceTask = nullptr;

	FScreenImageProperties ScreenImageProperties = { 0 };
	const FModelImageProperties ModelImageProperties = { 640, 480 };
	UNeuralNetwork* neuralNetwork = nullptr;
	UMyNeuralNetwork* myNeuralNetwork = nullptr;

	// todo: place below fields in a struct
	// count of total frames captured
	int frameCount = 1;
	// capture every frameMod frames
	int frameMod = 5;
	//should save images
	bool saveImages = false;

	int CanvasDrawCount=0;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "ImageCapture")
		void CaptureColorNonBlocking(USceneCaptureComponent2D* CaptureComponent, bool IsSegmentation = false);

	UFUNCTION(BlueprintCallable, Category = "ImageCapture", meta = (AllowPrivateAccess = "true"))
		void SetNeuralNetwork(UNeuralNetwork* Model);

	//get neural network
	UFUNCTION(BlueprintCallable, Category = "ImageCapture", meta = (AllowPrivateAccess = "true"))
		UNeuralNetwork* GetNeuralNetwork();

private:
	void SetupColorCaptureComponent(USceneCaptureComponent2D* CaptureComponent);
	void RunAsyncInferenceTask(const TArray<FColor>& RawImage, const FScreenImageProperties ScreenImage, const FModelImageProperties ModelImage, 
		UMyNeuralNetwork* MyNeuralNetwork);
};

class AsyncInferenceTask : public FNonAbandonableTask {
public:
	AsyncInferenceTask(const TArray<FColor>& RawImage, const FScreenImageProperties ScreenImage, const FModelImageProperties ModelImage, UMyNeuralNetwork* MyNeuralNetwork);

	~AsyncInferenceTask();

	// Required by UE4!
	FORCEINLINE TStatId GetStatId() const {
		RETURN_QUICK_DECLARE_CYCLE_STAT(AsyncInferenceTask, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	TArray<FColor> RawImageCopy;
	FScreenImageProperties ScreenImage;
	FModelImageProperties ModelImage;
	UMyNeuralNetwork* MyNeuralNetwork;

private:
	//	void ArrayFColorToUint8(const TArray<FColor>& RawImage, TArray<uint8>& InputImageCPU, int32 Width, int32 Height);
	void ResizeScreenImageToMatchModel(TArray<float>& ModelInputImage, TArray<uint8>& InputImageCPU,
		FModelImageProperties modelImage, FScreenImageProperties screenImage);
	void RunModel(TArray<float>& ModelInputImage, TArray<uint8>& ModelOutputImage);


public:
	void DoWork();
};