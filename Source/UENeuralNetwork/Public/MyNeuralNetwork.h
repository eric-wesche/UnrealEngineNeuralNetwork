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
};
