// Fill out your copyright notice in the Description page of Project Settings.


#include "CaptureManager.h"


#include "Engine.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"

#include "RHICommandList.h"

#include "Modules/ModuleManager.h"

#include "PreOpenCVHeaders.h"
#include "OpenCVHelper.h"
#include <ThirdParty/OpenCV/include/opencv2/imgproc.hpp>
#include <ThirdParty/OpenCV/include/opencv2/highgui/highgui.hpp>
#include <ThirdParty/OpenCV/include/opencv2/core.hpp>
#include "PostOpenCVHeaders.h"


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

void UCaptureManager::setNeuralNetwork(UNeuralNetwork* Model)
{
    //log model
    UE_LOG(LogTemp, Warning, TEXT("Model: %s"), *Model->GetName());
    Model->AddToRoot();
    UCaptureManager::myNeuralNetwork = NewObject<UMyNeuralNetwork>();
    //Model->SetDeviceType(ENeuralDeviceType::GPU); //gpu currently slower than cpu
    Model->SetDeviceType(ENeuralDeviceType::CPU);
    UCaptureManager::myNeuralNetwork->Network = Model;
}

UNeuralNetwork* UCaptureManager::getNeuralNetwork()
{
    return UCaptureManager::neuralNetwork;
}

void UCaptureManager::SetupColorCaptureComponent(USceneCaptureComponent2D* captureComponent) {
    // Create RenderTargets
    UTextureRenderTarget2D* renderTarget2D = NewObject<UTextureRenderTarget2D>();

    // Set FrameWidth and FrameHeight
    renderTarget2D->TargetGamma = 1.2f;// for Vulkan //GEngine->GetDisplayGamma(); // for DX11/12

    // Setup the RenderTarget capture format
    renderTarget2D->InitAutoFormat(256, 256); // some random format, got crashing otherwise
    int32 frameWidth = 640;
    int32 frameHeight = 480;
    renderTarget2D->InitCustomFormat(frameWidth, frameHeight, PF_B8G8R8A8, true); // PF_B8G8R8A8 disables HDR which will boost storing to disk due to less image information
    renderTarget2D->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
    renderTarget2D->bGPUSharedFlag = true; // demand buffer on GPU

    // Assign RenderTarget
    captureComponent->TextureTarget = renderTarget2D;

    // Set Camera Properties
    captureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    captureComponent->ShowFlags.SetTemporalAA(true);
    // lookup more showflags in the UE4 documentation..
}

void UCaptureManager::CaptureColorNonBlocking(USceneCaptureComponent2D* CaptureComponent, bool IsSegmentation) {
    if (!IsValid(CaptureComponent)) {
        UE_LOG(LogTemp, Error, TEXT("CaptureColorNonBlocking: CaptureComponent was not valid!"));
        return;
    }

    // Get RenderConterxt
    FTextureRenderTargetResource* renderTargetResource = CaptureComponent->TextureTarget->GameThread_GetRenderTargetResource();

    struct FReadSurfaceContext {
        FRenderTarget* SrcRenderTarget;
        TArray<FColor>* OutData;
        FIntRect Rect;
        FReadSurfaceDataFlags Flags;
    };

    // Init new RenderRequest
    FRenderRequest* renderRequest = new FRenderRequest();
    renderRequest->isPNG = IsSegmentation;

    int32 width = renderTargetResource->GetSizeXY().X;
    int32 height = renderTargetResource->GetSizeXY().Y;
    ScreenImage = { width, height };

    // Setup GPU command
    FReadSurfaceContext readSurfaceContext = {
        renderTargetResource,
        &(renderRequest->Image),
        FIntRect(0,0,renderTargetResource->GetSizeXY().X, renderTargetResource->GetSizeXY().Y),
        FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)
    };

    // Send command to GPU
   /* Up to version 4.22 use this
    ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        SceneDrawCompletion,//ReadSurfaceCommand,
        FReadSurfaceContext, Context, readSurfaceContext,
    {
        RHICmdList.ReadSurfaceData(
            Context.SrcRenderTarget->GetRenderTargetTexture(),
            Context.Rect,
            *Context.OutData,
            Context.Flags
        );
    });
    */
    // Above 4.22 use this
    ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
        [readSurfaceContext](FRHICommandListImmediate& RHICmdList) {
            RHICmdList.ReadSurfaceData(
                readSurfaceContext.SrcRenderTarget->GetRenderTargetTexture(),
                readSurfaceContext.Rect,
                *readSurfaceContext.OutData,
                readSurfaceContext.Flags
            );
        });

    // Notifiy new task in RenderQueue
    RenderRequestQueue.Enqueue(renderRequest);

    // Set RenderCommandFence
    // should pass true or false?
    renderRequest->RenderFence.BeginFence(false);
}

// Called every frame
void UCaptureManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!InferenceTaskQueue.IsEmpty()) {
        if (CurrentInferenceTask == nullptr || CurrentInferenceTask->IsDone()) {
            FAsyncTask<AsyncInferenceTask>* task = nullptr;
            InferenceTaskQueue.Dequeue(task);
            task->StartBackgroundTask();
            if (CurrentInferenceTask != nullptr && CurrentInferenceTask->IsDone()) delete CurrentInferenceTask;
            CurrentInferenceTask = task;
        }
    }

    if (frameCount++ % frameMod == 0) {
        // Capture Color Image (adds render request to queue)
        CaptureColorNonBlocking(ColorCaptureComponents, false);
        frameCount = 1;
    }
    // Read pixels once RenderFence is completed
    if (!RenderRequestQueue.IsEmpty()) {

        // Peek the next RenderRequest from queue
        FRenderRequest* nextRenderRequest = nullptr;
        RenderRequestQueue.Peek(nextRenderRequest);

        if (nextRenderRequest) { //nullptr check
            if (nextRenderRequest->RenderFence.IsFenceComplete()) { // Check if rendering is done, indicated by RenderFence

                // create and enqueue new inference task
                FAsyncTask<AsyncInferenceTask>* MyTask =
                    new FAsyncTask<AsyncInferenceTask>(nextRenderRequest->Image, ScreenImage, ModelImage, myNeuralNetwork);
                InferenceTaskQueue.Enqueue(MyTask);

                // Delete the first element from RenderQueue
                RenderRequestQueue.Pop();
                delete nextRenderRequest;

            }
        }
    }
}

//create function for run inference task. call this when get the frame
void UCaptureManager::RunAsyncInferenceTask(const TArray<FColor> RawImage, const FScreenImage _ScreenImage, const FModelImage _ModelImage, 
    UMyNeuralNetwork* MyNeuralNetwork) {
    (new FAutoDeleteAsyncTask<AsyncInferenceTask>(RawImage, _ScreenImage, _ModelImage, MyNeuralNetwork))->StartBackgroundTask();
}

//initialize image
AsyncInferenceTask::AsyncInferenceTask(const TArray<FColor> RawImage, const FScreenImage ScreenImage,
    const FModelImage ModelImage, UMyNeuralNetwork* MyNeuralNetwork) {
    this->RawImageCopy = RawImage;
    this->ScreenImage = ScreenImage;
    this->ModelImage = ModelImage;
    this->MyNeuralNetwork = MyNeuralNetwork;
}

AsyncInferenceTask::~AsyncInferenceTask() {
    //UE_LOG(LogTemp, Warning, TEXT("AsyncTaskDone inference"));
}

//do inference
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
    FModelImage modelImage, FScreenImage screenImage)
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
    check(MyNeuralNetwork);
    ModelOutputImage.Reset();
    MyNeuralNetwork->URunModel(ModelInputImage, ModelOutputImage);
}