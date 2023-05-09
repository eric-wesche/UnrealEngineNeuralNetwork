// Fill out your copyright notice in the Description page of Project Settings.

#include "MyNeuralNetwork.h"

UMyNeuralNetwork::UMyNeuralNetwork()
{
	Network = nullptr;
}

uint8 BBFloatToColor(float value) {
	return static_cast<uint8>(FMath::Clamp(value, 0, 255));
}

void UMyNeuralNetwork::URunModel(TArray<float>& image, TArray<uint8>& results)
{
	if (Network == nullptr || !Network->IsLoaded()) {
		UE_LOG(LogTemp, Error, TEXT("Neural Network not loaded."));
		return;
	}

	// start timer
	double startSeconds = FPlatformTime::Seconds();

	//log the size of the input image
	//UE_LOG(LogTemp, Display, TEXT("Input image size: %d."), image.Num());

	// Running inference
	Network->SetInputFromArrayCopy(image);

	// Run UNeuralNetwork
	Network->Run();

	FNeuralTensor pOutputTensor = Network->GetOutputTensor();


	//{1 84 6300} -- yolov8 output width image 640x480. 6300 predictions. 4 box coordinates + 80 class probabilities

	TArray<float> arr = Network->GetOutputTensor().GetArrayCopy<float>();

	// get size of each dimension of tensor
	auto sizes = pOutputTensor.GetSizes();
	//loop through each column of 1d array row by row
	int total = arr.Num();
	int columns = sizes[1];
	int rows = total / columns;
	//for (int i = 0; i < 5; i++) {
	for (int i = 0; i < rows; i++) {
		TArray<float> predictions;
		TArray<float> boxCoordinates;
		for (int j = 0; j < columns; j++) {
			float cell = arr[i + j * rows];
			if (j < 4) {
				boxCoordinates.Add(cell);
			}
			else {
				predictions.Add(cell);
			}
		}
		// print the predictions
		for (int k = 0; k < predictions.Num(); k++) {
			auto cell = predictions[k];
			if (cell > 0.8) {
				UE_LOG(LogTemp, Warning, TEXT("class: %d, probability: %f"), k, cell);
			}
		}
	}


	// print time elapsed
	double secondsElapsed = FPlatformTime::Seconds() - startSeconds;

	//UE_LOG(LogTemp, Log, TEXT("Results created successfully in %f."), secondsElapsed)
}