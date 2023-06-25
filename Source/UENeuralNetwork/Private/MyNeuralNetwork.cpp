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


	// {1 84 6300} -- yolov8 output image 640x480. 6300 predictions. 4 box coordinates + 80 class probabilities
	// yolov8 has three output layers with strides 8, 16, 32; it predicts one bounding box per cell; 
	// so, the number of predictions is equal to the number of cells in the output layers.
	// 640/8 = 80, 480/8 = 60. 80x60 = 4800.
	// 640/16 = 40, 480/16 = 30. 40x30 = 1200.
	// 640/32 = 20, 480/32 = 15. 20x15 = 300.
	// 4800 + 1200 + 300 = 6300 predictions.

	// the flattened output tensor is 84 groups of 6300 values.
	TArray<float> arr = Network->GetOutputTensor().GetArrayCopy<float>();

	// get size of each dimension of tensor
	auto sizes = pOutputTensor.GetSizes();
	int total = arr.Num();
	int columns = sizes[1];
	int rows = total / columns;

	// store array of size 80 of highest class predictions for the image
	TArray<float> highestClassPredictions;
	for (int i = 0; i < 80; i++) {
		highestClassPredictions.Add(0);
	}

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
		// print the predictions (should be 80)
		for (int k = 0; k < predictions.Num(); k++) {
			auto cell = predictions[k];
			if (cell > highestClassPredictions[k]) {
				highestClassPredictions[k] = cell;
			}
			if (cell > 0.8) {
				//UE_LOG(LogTemp, Warning, TEXT("class: %d, probability: %f"), k, cell);
			}
		}
	}
	
	// print the classes that have a probability greater than 0.8
	for (int i = 0; i < highestClassPredictions.Num(); i++) {
		auto cell = highestClassPredictions[i];
		if (cell > 0.8) {
			UE_LOG(LogTemp, Warning, TEXT("class: %d, probability: %f"), i, cell);
		}
	}

	// print time elapsed
	double secondsElapsed = FPlatformTime::Seconds() - startSeconds;

	//UE_LOG(LogTemp, Log, TEXT("Results created successfully in %f."), secondsElapsed)
}