# Object-Detection-MicroService-CPP
Object Detection MicroService C++

# The Idea
- The idea is to create self-organizing micro-services with very loose coupling. By self-organizing I mean micro-services that are aware of their workload and based on that they decide how much new sources can handle. If it happens that for some reason microservice cannot handle as many sources as it declared it is obligated to push this source to the 'available sources' queue so another microservice (if available) can handle it.
- The source can also be marked as ``obsolete`` and should be dropped immediately and no longer be processed by the service.
- The primary aim is performance, and the ability to deploy this solution into any system.
  

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
- [OpenCV 4.7.0](https://github.com/opencv/opencv/tree/4.7.0) with CUDA & cuDNN support (GPU computing)
- [AMQP-CPP](https://github.com/CopernicaMarketingSoftware/AMQP-CPP)
- [Boost 1.82.0](https://www.boost.org/users/history/version_1_82_0.html) ([Getting Started](https://www.boost.org/doc/libs/1_82_0/more/getting_started/unix-variants.html))
```
wget https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.tar.gz
tar -xf boost_1_82_0.tar.gz
rm -dr boost_1_82_0.tar.gz
cd boost_1_82_0
sudo ./bootstrap.sh
sudo ./b2 install
```

# Platform
The currently supported platforms are ``POSIX based`` only.
It is due to limitations in AMQP-CPP TcpHandler. (It is possible to support Windows by implementing TcpHandler for Windows)

# Build
```
cd ~
git clone https://github.com/TheYoungBeast/Object-Detection-MicroService-CPP.git
cd Object-Detection-MicroService-CPP
mkdir build && cd build
cmake ..
make
```
  
# To Do:
- Add logger
- Elasticsearch?
- Add docker container
- Add JSON Support or Protobuf for messages (que messages)
- Refactor code
- Support at least Yolo V5 & Yolo V8

# Contact?
Find me on: <br />
Discord: ``ruhrohraggyretard``
