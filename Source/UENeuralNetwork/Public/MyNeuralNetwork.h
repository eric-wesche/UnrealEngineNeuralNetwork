// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NeuralNetwork.h"
#include "MyNeuralNetwork.generated.h"

/**
 * 
 */
UCLASS()
class UENEURALNETWORK_API UMyNeuralNetwork : public UNeuralNetwork
{
	GENERATED_BODY()
public:
	UPROPERTY(Transient)
		UNeuralNetwork* Network = nullptr;
	UMyNeuralNetwork();
	void URunModel(TArray<float>& image, TArray<uint8>& results);

	//define struct
	struct FBoxCoordinates {
		float cx; // center x
		float cy; // center y
		float width;
		float height;
		float x1; // top left x
		float y1; // top left y
		float confidence = 0.0f;
	};

	TMap<int, TArray<FBoxCoordinates>> BoundingBoxCoordinatesMap;
	float ConfidenceThreshold = 0.65f;

	// Define a function that takes a file path as a parameter and returns a TMap
	static TMap<int, FString> ReadFileToMap(FString FilePath);

	TMap<int, FString> CocoDatasetClassIntToStringMap;
};
