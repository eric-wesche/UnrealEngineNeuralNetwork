// Fill out your copyright notice in the Description page of Project Settings.


#include "CaptureManager.h"

#include "Engine.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"

#include "RHICommandList.h"

#include "Modules/ModuleManager.h"

#include "PreOpenCVHeaders.h"
#include <ThirdParty/OpenCV/include/opencv2/imgproc.hpp>
#include <ThirdParty/OpenCV/include/opencv2/core.hpp>

#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/AssertionMacros.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UENeuralNetwork/UENeuralNetworkGameMode.h"

// statics
UMaterialInstanceDynamic* UCaptureManager::DynamicMaterialInstance = nullptr;
UCanvasRenderTarget2D* UCaptureManager::BoundingBoxRenderTarget2D = nullptr;
UMyNeuralNetwork::FBoxCoordinates UCaptureManager::BoundingBoxCoordinates = UMyNeuralNetwork::FBoxCoordinates();

// Sets default values for this component's properties
UCaptureManager::UCaptureManager()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = true;
}

// Called when the game starts
void UCaptureManager::BeginPlay()
{
    Super::BeginPlay();

    //return if ColorCaptureComponents is not set
    if (!ColorCaptureComponents) {
        UE_LOG(LogTemp, Warning, TEXT("ColorCaptureComponents not set"));
        return;
    }
    SetupColorCaptureComponent(ColorCaptureComponents);
}

namespace {
    void ArrayFColorToUint8(const TArray<FColor>& RawImage, TArray<uint8>& InputImageCPU, int32 Width, int32 Height) {
        const int PixelCount = Width * Height;
        InputImageCPU.SetNumZeroed(PixelCount * 3);

        ParallelFor(RawImage.Num(), [&](int32 Idx) {
            const int i = Idx * 3;
            const FColor& Pixel = RawImage[Idx];

            InputImageCPU[i] = Pixel.R;
            InputImageCPU[i + 1] = Pixel.G;
            InputImageCPU[i + 2] = Pixel.B;
            });
    }
}

void UCaptureManager::SetNeuralNetwork(UNeuralNetwork* Model)
{
    //log model
    UE_LOG(LogTemp, Warning, TEXT("Model: %s"), *Model->GetName());
    Model->AddToRoot();
    UCaptureManager::myNeuralNetwork = NewObject<UMyNeuralNetwork>();
    //Model->SetDeviceType(ENeuralDeviceType::GPU); //gpu currently slower than cpu
    Model->SetDeviceType(ENeuralDeviceType::CPU);
    UCaptureManager::myNeuralNetwork->Network = Model;
}

UNeuralNetwork* UCaptureManager::GetNeuralNetwork()
{
    return UCaptureManager::neuralNetwork;
}

void UCaptureManager::OnCanvasRenderTargetUpdate2(UCanvas* Canvas, int32 Width, int32 Height) {
    float x = BoundingBoxCoordinates.x1;
    float y = BoundingBoxCoordinates.y1;
    float width = BoundingBoxCoordinates.width;
    float height = BoundingBoxCoordinates.height;
    Canvas->K2_DrawBox(FVector2D(x, y), FVector2D(width, height), 5, FLinearColor::Red);
}

/**
 * @brief Initializes the render targets and material
 */
void UCaptureManager::SetupColorCaptureComponent(USceneCaptureComponent2D* CaptureComponent) {
    UObject* worldContextObject = GetWorld();

    // scene capture component render target (stores frame that is then pulled from gpu to cpu for neural network input)
    RenderTarget2D = NewObject<UTextureRenderTarget2D>();
    RenderTarget2D->InitAutoFormat(256, 256); // some random format, got crashing otherwise
    RenderTarget2D->InitCustomFormat(ModelImageProperties.width, ModelImageProperties.height, PF_B8G8R8A8, true); // PF_B8G8R8A8 disables HDR which will boost storing to disk due to less image information
    RenderTarget2D->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
    RenderTarget2D->bGPUSharedFlag = true; // demand buffer on GPU
    RenderTarget2D->TargetGamma = 1.2f;// for Vulkan //GEngine->GetDisplayGamma(); // for DX11/12

    // add render target to scene capture component
    CaptureComponent->TextureTarget = RenderTarget2D;

    // setup material and add render target to material
    UMaterial* material = LoadObject<UMaterial>(nullptr, TEXT("/Script/Engine.Material'/Game/ThirdPerson/Blueprints/NewTextureRenderTarget2D_Mat.NewTextureRenderTarget2D_Mat'"));
    UWorld* world = GetWorld();
    AGameModeBase* gameMode = world->GetAuthGameMode();
    AUENeuralNetworkGameMode* myGameMode = Cast<AUENeuralNetworkGameMode>(gameMode);
    myGameMode->GMDynamicMaterialInstance = UMaterialInstanceDynamic::Create(material, nullptr);;  
    myGameMode->GMDynamicMaterialInstance->SetTextureParameterValue("TextureParam", RenderTarget2D);

    // bounding box
    BoundingBoxRenderTarget2D = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(
    worldContextObject, UCanvasRenderTarget2D::StaticClass(), 256, 256);
    BoundingBoxRenderTarget2D->OnCanvasRenderTargetUpdate.AddDynamic(this, &UCaptureManager::OnCanvasRenderTargetUpdate2);
    BoundingBoxRenderTarget2D->InitAutoFormat(256, 256); // some random format, got crashing otherwise
    BoundingBoxRenderTarget2D->InitCustomFormat(ModelImageProperties.width, ModelImageProperties.height, PF_B8G8R8A8, true); 
    BoundingBoxRenderTarget2D->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
    BoundingBoxRenderTarget2D->bGPUSharedFlag = true; // demand buffer on GPU
    BoundingBoxRenderTarget2D->TargetGamma = 1.2f;// for Vulkan //GEngine->GetDisplayGamma(); // for DX11/12
    myGameMode->GMDynamicMaterialInstance->SetTextureParameterValue("BoundingBoxTextureParam", BoundingBoxRenderTarget2D);

    // Set Camera Properties
    CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    CaptureComponent->ShowFlags.SetTemporalAA(true);
}

/**
 * @brief Sends request to gpu to read frame (send from gpu to cpu)
 * @param CaptureComponent 
 * @param IsSegmentation 
 */
void UCaptureManager::CaptureColorNonBlocking(USceneCaptureComponent2D* CaptureComponent, bool IsSegmentation) {
    if (!IsValid(CaptureComponent)) {
        UE_LOG(LogTemp, Error, TEXT("CaptureColorNonBlocking: CaptureComponent was not valid!"));
        return;
    }

    FTextureRenderTargetResource* renderTargetResource = CaptureComponent->TextureTarget->GameThread_GetRenderTargetResource();
    
    const int32& rtx = CaptureComponent->TextureTarget->SizeX;
    const int32& rty = CaptureComponent->TextureTarget->SizeY;
    
    struct FReadSurfaceContext {
        FRenderTarget* SrcRenderTarget;
        TArray<FColor>* OutData;
        FIntRect Rect;
        FReadSurfaceDataFlags Flags;
    };

    // Init new RenderRequest
    FRenderRequest* renderRequest = new FRenderRequest();
    renderRequest->isPNG = IsSegmentation;

    int32 width = rtx; 
    int32 height = rty;
    ScreenImageProperties = { width, height };

    // Setup GPU command. send the same command again but use the render target that is in the widget, and modify it to add the box
    FReadSurfaceContext readSurfaceContext = {
        renderTargetResource,
        &(renderRequest->Image), // store frame in render request
        FIntRect(0,0,width, height),
        FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)
    };
    
    ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
        [readSurfaceContext](FRHICommandListImmediate& RHICmdList) {
            RHICmdList.ReadSurfaceData(
                readSurfaceContext.SrcRenderTarget->GetRenderTargetTexture(),
                readSurfaceContext.Rect,
                *readSurfaceContext.OutData,
                readSurfaceContext.Flags
            );
        });

    // Add new task to RenderQueue
    RenderRequestQueue.Enqueue(renderRequest);

    // Set RenderCommandFence
    // TODO: should pass true or false?
    renderRequest->RenderFence.BeginFence(false);
}

/**
 * @brief If scene component is not running every frame, and this function is, then it will be reading the same data
 * from the texture over and over. TODO: check if this is true, and ensure no issues.
 * @param DeltaTime 
 * @param TickType 
 * @param ThisTickFunction
 */
void UCaptureManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!InferenceTaskQueue.IsEmpty()) { // Check if there is a task in queue and start it, and delete the old one
        if (CurrentInferenceTask == nullptr || CurrentInferenceTask->IsDone()) {
            FAsyncTask<AsyncInferenceTask>* task = nullptr;
            InferenceTaskQueue.Dequeue(task);
            task->StartBackgroundTask();
            if (CurrentInferenceTask != nullptr && CurrentInferenceTask->IsDone()) delete CurrentInferenceTask;
            CurrentInferenceTask = task;
        }
    }

    if (frameCount++ % frameMod == 0) { // capture every frameMod frame
        // Capture Color Image (adds render request to queue)
        CaptureColorNonBlocking(ColorCaptureComponents, false);
        frameCount = 1;
    }
    // If there is a render task in the queue, read pixels once RenderFence is completed
    if (!RenderRequestQueue.IsEmpty()) {
        // Peek the next RenderRequest from queue
        FRenderRequest* nextRenderRequest = nullptr;
        RenderRequestQueue.Peek(nextRenderRequest);
        if (nextRenderRequest) { // nullptr check
            if (nextRenderRequest->RenderFence.IsFenceComplete()) { // Check if rendering is done, indicated by RenderFence
                // we have the image, now we draw a box around the detected object and display it on the screen
                // render image to render target
                UKismetRenderingLibrary::ExportRenderTarget(GEngine->GetWorld(), RenderTarget2D, "C:\\ueimages", "test.png");
                // renderTarget2D->UpdateResource(); // if update before saving to image it will be black
                // create and enqueue new inference task
                FAsyncTask<AsyncInferenceTask>* MyTask =
                    new FAsyncTask<AsyncInferenceTask>(nextRenderRequest->Image, ScreenImageProperties, ModelImageProperties, myNeuralNetwork);
                InferenceTaskQueue.Enqueue(MyTask);
                // Delete the first element from RenderQueue
                RenderRequestQueue.Pop();
                delete nextRenderRequest;
            }
        }
    }

    BoundingBoxRenderTarget2D->UpdateResource();
}

// create function for run inference task. call this when get the frame
void UCaptureManager::RunAsyncInferenceTask(const TArray<FColor>& RawImage, const FScreenImageProperties _ScreenImage, const FModelImageProperties _ModelImage, 
    UMyNeuralNetwork* MyNeuralNetwork) {
    (new FAutoDeleteAsyncTask<AsyncInferenceTask>(RawImage, _ScreenImage, _ModelImage, MyNeuralNetwork))->StartBackgroundTask();
}

// initialize image
AsyncInferenceTask::AsyncInferenceTask(const TArray<FColor>& RawImage, const FScreenImageProperties ScreenImage,
    const FModelImageProperties ModelImage, UMyNeuralNetwork* MyNeuralNetwork) {
    this->RawImageCopy = RawImage;
    this->ScreenImage = ScreenImage;
    this->ModelImage = ModelImage;
    this->MyNeuralNetwork = MyNeuralNetwork;
}

AsyncInferenceTask::~AsyncInferenceTask() {
    //UE_LOG(LogTemp, Warning, TEXT("AsyncTaskDone inference"));
}

// do inference
void AsyncInferenceTask::DoWork() {
    //log do work
    //UE_LOG(LogTemp, Warning, TEXT("AsyncTaskDoWork inference"));

    //convert image to uint8
    TArray<uint8> InputImageCPU;
    ArrayFColorToUint8(RawImageCopy, InputImageCPU, ScreenImage.width, ScreenImage.height);

    //declare model input image
    TArray<float> ModelInputImage;

    //resize image to match model
    ResizeScreenImageToMatchModel(ModelInputImage, InputImageCPU, ModelImage, ScreenImage);

    //declare model output image
    TArray<uint8> ModelOutputImage;

    //run inference
    RunModel(ModelInputImage, ModelOutputImage);
}

void AsyncInferenceTask::ResizeScreenImageToMatchModel(TArray<float>& ModelInputImage, TArray<uint8>& InputImageCPU,
    FModelImageProperties modelImage, FScreenImageProperties screenImage)
{
    // Create image from StylizedImage object
    cv::Mat inputImage(screenImage.height, screenImage.width, CV_8UC3, InputImageCPU.GetData());


    // Create image to resize for inferencing
    cv::Mat outputImage(modelImage.height, modelImage.width, CV_8UC3);

    // Resize to outputImage
    cv::resize(inputImage, outputImage, cv::Size(modelImage.width, modelImage.height));

    // Reshape to 1D
    outputImage = outputImage.reshape(1, 1);

    // uint_8, [0, 255] -> float, [0, 1]
    std::vector<float> vec;
    outputImage.convertTo(vec, CV_32FC1, 1. / 255);


    // Height, Width, Channel to Channel, Height, Width
    const int inputSize = modelImage.height * modelImage.width * 3;
    ModelInputImage.Reset();
    ModelInputImage.SetNumZeroed(inputSize);

    const int blockSize = inputSize / 3;

    //ModelInputImage stores in 3 chunks, one for each channel (RGB), each chunk is blockSize
    for (size_t ch = 0; ch < 3; ++ch) {
        //this runs blockSize times, each run at the same time
        ParallelFor(blockSize, [&](int32 Idx) {
            //vec stores values in blockSize chunks, each chunk is a pixel (RGB). This gets the ch value for the current pixel
            const int i = (Idx * 3) + ch;

            //this is so we can fill the right channel (each size blockSize). eg fill the first blockSize elements with R values, then next channel 
            //starts at element with index of blockSize and fills the G values. Then the next channel starts at element with index of blockSize * 2 and 
            //fills the B values
            const int stride = ch * blockSize;

            ModelInputImage[Idx + stride] = vec[i];
            });
    }
}

void AsyncInferenceTask::RunModel(TArray<float>& ModelInputImage, TArray<uint8>& ModelOutputImage) {
    // check(MyNeuralNetwork);
    if(MyNeuralNetwork == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("MyNeuralNetwork is null"));
        return;
    }
    ModelOutputImage.Reset();
    MyNeuralNetwork->URunModel(ModelInputImage, ModelOutputImage);
}