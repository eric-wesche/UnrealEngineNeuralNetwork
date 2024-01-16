# Demo

See below gifs showing the bounding boxes that yolov8 nano predicts using an image of width 640px and height 480px (this is what is input into the model and also the size of the image in the top left of the screen with the bounding boxes). The camera is on the front of the white cylinder. 

![](https://github.com/eric-wesche/UnrealEngineNeuralNetwork/blob/master/demo.gif)

This one has labels. You can see it sometimes predicts the class wrong; I set the confidence threshold for showing the boxes to .65.

![](https://github.com/eric-wesche/UnrealEngineNeuralNetwork/blob/master/example_with_labels_2.gif)

# UnrealEngineNeuralNetwork

Developed with Unreal Engine 5

This uses the Neural Network Inference (NNI) plugin by Epic Games and Microsoft to run the yolov8n onnx model in Unreal Engine 5. It can easily be extended to use other models.

I used code from here to read and process frames in Unreal Engine asynchronously: https://github.com/TimmHess/UnrealImageCapture

I gained understanding from this for how to use the NNI plugin: https://medium.com/@weirdframes/bringing-deep-learning-to-unreal-engine-5-pt-2-51c1a2a2c3

This thread helped me understand the outputs of yolov8: https://github.com/ultralytics/ultralytics/issues/751

# Setup

Set the Default GameMode to UENeuralNetworkGameMode (the C++ class). The humanoid character asset and map are from using the Third Person Template (C++, include Starter Content) and the white cylinder is from Starter Content.  
