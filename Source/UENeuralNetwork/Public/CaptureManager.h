// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CoreMinimal.h"

#include "Components/SceneCaptureComponent2D.h"

#include "NeuralNetwork.h"
#include "MyNeuralNetwork.h"

#include "Components/ActorComponent.h"
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
struct FScreenImage {
	GENERATED_BODY()

	int32 width;
	int32 height;
};

USTRUCT()
struct FModelImage {
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

	// PostProcessMaterial used for segmentation
	UPROPERTY(EditAnywhere, Category = "Segmentation Setup")
		UMaterial* PostProcessMaterial = nullptr;

private:
	// RenderRequest Queue
	TQueue<FRenderRequest*> RenderRequestQueue;
	// inference task queue
	TQueue<FAsyncTask<AsyncInferenceTask>*> InferenceTaskQueue;
	// current inference task
	FAsyncTask<AsyncInferenceTask>* CurrentInferenceTask = nullptr;

	FScreenImage ScreenImage = { 0 };
	const FModelImage ModelImage = { 640, 480 };
	UNeuralNetwork* neuralNetwork = nullptr;
	UMyNeuralNetwork* myNeuralNetwork = nullptr;

	// todo: place below fields in a struct
	// count of total frames captured
	int frameCount = 1;
	// capture every frameMod frames
	int frameMod = 5;
	//should save images
	bool saveImages = false;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "ImageCapture")
		void CaptureColorNonBlocking(USceneCaptureComponent2D* CaptureComponent, bool IsSegmentation = false);

	UFUNCTION(BlueprintCallable, Category = "ImageCapture", meta = (AllowPrivateAccess = "true"))
		void setNeuralNetwork(UNeuralNetwork* Model);

	//get neural network
	UFUNCTION(BlueprintCallable, Category = "ImageCapture", meta = (AllowPrivateAccess = "true"))
		UNeuralNetwork* getNeuralNetwork();

private:
	void SetupColorCaptureComponent(USceneCaptureComponent2D* captureComponent);
	void RunAsyncInferenceTask(const TArray<FColor> RawImage, const FScreenImage ScreenImage, const FModelImage ModelImage, 
		UMyNeuralNetwork* MyNeuralNetwork);
};

class AsyncInferenceTask : public FNonAbandonableTask {
public:
	AsyncInferenceTask(const TArray<FColor> RawImage, const FScreenImage ScreenImage, const FModelImage ModelImage, UMyNeuralNetwork* MyNeuralNetwork);

	~AsyncInferenceTask();

	// Required by UE4!
	FORCEINLINE TStatId GetStatId() const {
		RETURN_QUICK_DECLARE_CYCLE_STAT(AsyncInferenceTask, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	TArray<FColor> RawImageCopy;
	FScreenImage ScreenImage;
	FModelImage ModelImage;
	UMyNeuralNetwork* MyNeuralNetwork;

private:
	//	void ArrayFColorToUint8(const TArray<FColor>& RawImage, TArray<uint8>& InputImageCPU, int32 Width, int32 Height);
	void ResizeScreenImageToMatchModel(TArray<float>& ModelInputImage, TArray<uint8>& InputImageCPU,
		FModelImage modelImage, FScreenImage screenImage);
	void RunModel(TArray<float>& ModelInputImage, TArray<uint8>& ModelOutputImage);


public:
	void DoWork();
};