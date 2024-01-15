// Fill out your copyright notice in the Description page of Project Settings.

#include "MyNeuralNetwork.h"

#include "CaptureManager.h"

UMyNeuralNetwork::UMyNeuralNetwork()
{
	Network = nullptr;

	FString RelativePath = FPaths::GameSourceDir();
	RelativePath = FPaths::Combine(RelativePath, TEXT("UENeuralNetwork"), TEXT("Public"), TEXT("coco_classes.txt"));
	CocoDatasetClassIntToStringMap = ReadFileToMap(RelativePath);
	UCaptureManager::CocoDatasetClassIntToStringMap = CocoDatasetClassIntToStringMap;
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

	// Clear the bounding box coordinates map
	BoundingBoxCoordinatesMap.Empty();

	// start timer to see how long this function takes
	double startSeconds = FPlatformTime::Seconds();

	Network->SetInputFromArrayCopy(image);

	// Run UNeuralNetwork inference
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

	const int numClasses = 80; // number of classes the model predicts
	
	// store the bounding box coordinates for each class as a struct
	TArray<FBoxCoordinates> boundingBoxCoordinates;
	for (int i = 0; i < numClasses; i++) {
		boundingBoxCoordinates.Add(FBoxCoordinates());
	}

	int highestClass = -1;
	// loop through the flattened tensor and populate the boundingBoxCoordinates array
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
			float ClassConfidencePrediction = predictions[k];
			// log conf
			if(ClassConfidencePrediction > ConfidenceThreshold)
			{
				FBoxCoordinates coords = FBoxCoordinates();
				coords.confidence = ClassConfidencePrediction;
				coords.cx = boxCoordinates[0];
				coords.cy = boxCoordinates[1];
				coords.width = boxCoordinates[2];
				coords.height = boxCoordinates[3];
				coords.x1 = coords.cx - (coords.width / 2);
				coords.y1 = coords.cy - (coords.height / 2);
				BoundingBoxCoordinatesMap.FindOrAdd(k).Add(coords);
			}
			// auto cell = predictions[k];
			// if (cell > boundingBoxCoordinates[k].confidence) {
			// 	boundingBoxCoordinates[k].confidence = cell;
			//
			// 	// get cx, cy, w, h from the tensor output
			// 	float cx = boxCoordinates[0];
			// 	float cy = boxCoordinates[1];
			// 	float w = boxCoordinates[2];
			// 	float h = boxCoordinates[3];
			// 	boundingBoxCoordinates[k].cx = cx;
			// 	boundingBoxCoordinates[k].cy = cy;
			// 	boundingBoxCoordinates[k].width = w;
			// 	boundingBoxCoordinates[k].height = h;
			// 	// get the top left corner of the bounding box given the center (cx,cy) and width and height
			// 	float x1 = cx - (w / 2);
			// 	float y1 = cy - (h / 2);
			// 	boundingBoxCoordinates[k].x1 = x1;
			// 	boundingBoxCoordinates[k].y1 = y1;
			//
			// 	if(highestClass == -1 || cell>boundingBoxCoordinates[highestClass].confidence){
			// 		highestClass = k;
			// 	}
			// }
		}
	}

	// if(highestClass > -1)
	// {
	// 	FBoxCoordinates coords = boundingBoxCoordinates[highestClass];
	// 	UCaptureManager::BoundingBoxCoordinates = coords;
	//
	// 	// print highest class prediction and its probability
	// 	// UE_LOG(LogTemp, Warning, TEXT("class: %d, probability: %f"), highestClass, boundingBoxCoordinates[highestClass].confidence);
	// }

	UCaptureManager::BoundingBoxCoordinatesMap = BoundingBoxCoordinatesMap;
	
	// print time elapsed for this function
	double secondsElapsed = FPlatformTime::Seconds() - startSeconds;

	//UE_LOG(LogTemp, Log, TEXT("Results created successfully in %f."), secondsElapsed)
}

TMap<int, FString> UMyNeuralNetwork::ReadFileToMap(FString FilePath)
{
	// Create an empty TMap with int as the key type and FString as the value type
	TMap<int, FString> Result;

	// Check if the file exists
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
	{
		// Create an empty FString to store the file content
		FString FileContent;

		// Load the file content to the FString
		FFileHelper::LoadFileToString(FileContent, *FilePath);

		// Split the file content by line breaks into an array of strings
		TArray<FString> Lines;
		FileContent.ParseIntoArray(Lines, TEXT("\n"), true);

		// Loop through each line in the array
		for (FString Line : Lines)
		{
			// Split the line by colon into an array of strings
			TArray<FString> KeyValue;
			Line.ParseIntoArray(KeyValue, TEXT(":"), true);

			// Check if the array has two elements
			if (KeyValue.Num() == 2)
			{
				// Convert the first element to an int and trim any whitespace
				int Key = FCString::Atoi(*KeyValue[0].TrimStartAndEnd());

				// Get the second element and trim any whitespace
				FString Value = KeyValue[1].TrimStartAndEnd();

				// Add the key-value pair to the map
				Result.Add(Key, Value);
			}
		}
	}

	// Return the map
	return Result;
}


