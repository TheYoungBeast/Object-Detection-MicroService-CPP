# Object-Detection-MicroService-CPP
Object Detection MicroService C++

# Architecture Assumption
- Use RabbitMQ Server/Client
- Single instance of Background Service (Detection Service in this case)
- Send byte data frame to Queue (any image size) -> decode (to cv::Mat) -> add to appropriate service's queue -> detect & preprocess in the background -> [publish results - not implemented yet]
- Thread-safe operations
- Let the service measure performance and automatically decide to register new sources or unregister if the source is causing too much load which results in increasing delay. In unregistering, the "source" goes back to the "AVAILABLE_SOURCES" queue so another instance of MicroService can handle it.

# The Goal
- This is only one element of a larger System
- Hit at least 100+FPS or ~200+FPS with lighter model on one machine
- Parallel processing
- Solid improved architecture
- Completely switch to GPU algorithms to further improve the performance

# Dependencies
- OpenCV 4.7.0 with CUDA & cuDNN support (To use GPU computing)
- [AMQP-CPP](https://github.com/CopernicaMarketingSoftware/AMQP-CPP)
- Boost
  
# To Do:
- Add logger
- Add docker container
- Add JSON Support or Protobuf for messages (que messages)
- Refactor code
- Support at least Yolo V5 & Yolo V8

# Contact?
Find me on: <br />
Discord: ``ruhrohraggyretard``
