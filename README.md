# Object-Detection-MicroService-CPP
Event-driven microservice architecture. The microservice is meant to be scaled horizontally. Object Detection MicroService C++.
Current performance ~``80FPS`` single service instance. (GTX 1080Ti)

# Installation
Deploy options are available below. <br>
Please bear in mind that the project is currently in progress and may not perform as you would expect.
If you plan to develop this piece of software move to the [build & run](https://github.com/TheYoungBeast/Object-Detection-MicroService-CPP#build&run) section.

## Docker
Build the image from the GitHub repository:
```
 docker build -t microservice/object-detection https://github.com/TheYoungBeast/Object-Detection-MicroService-CPP.git
```

Then run the image:
```
docker run --gpus all -it microservice/object-detection:latest
```

## Docker Compose
I'll add it soon

## Requirements
Bear in mind that you need Nvidia GPU to use this piece of software. It is because it uses CUDA/CuDNN hence the requirements.

## Note
If there's an error saying that the CUDA backend (or anything with GPU) is not available - try to rebuild the docker image specifying ``CUDA_ARCH_BIN`` arg. Go to [CUDA GPUs - Compute Capability](https://developer.nvidia.com/cuda-gpus) site and find your GPU model and compute capability number. (In my case it's 6.1 for GTX 1080 Ti) Then modify the dockerfile either by hand or by build-args and set CUDA_ARCH_BIN="Your compute capability" e.g  CUDA_ARCH_BIN="6.1"  and rebuild it.

# The Idea
- The idea is to create self-organizing micro-services with very loose coupling. By self-organizing I mean micro-services that are aware of their workload and based on that they decide how much new sources can handle. If it happens that the microservice cannot hold as many sources as it declared it is obligated to push this source to the 'available sources' queue so another microservice (if available) can handle it.
- The source can also be marked as ``obsolete`` and should be dropped immediately and no longer be processed by the service.
- The primary aim is performance, and the ability to deploy this solution into any system.
  
# Architecture Assumption
- Use RabbitMQ Server/Client
- Single instance of Background Service (Detection Service in this case)
- Send byte data frame to Queue (any image size) -> decode (to cv::Mat) -> add to appropriate service's queue -> detect & preprocess in the background -> [publish results - not implemented yet]
- Thread-safe operations
- Let the service measure performance and automatically decide to register new sources or unregister if the source is causing too much load which results in increasing delay. In unregistering, the "source" goes back to the "AVAILABLE_SOURCES" queue so another instance of MicroService can handle it.

![obraz](https://github.com/TheYoungBeast/Object-Detection-MicroService-CPP/assets/19922252/fe0d9684-03ff-4659-911f-c88d0b40fd12)


# The Goal
- This is only one element of a larger System
- Hit at least 100+FPS or ~200+FPS with lighter model on one machine
- Parallel processing
- Solid improved architecture
- Completely switch to GPU algorithms to further improve the performance

# Dependencies
- [OpenCV 4.7.0](https://github.com/opencv/opencv/tree/4.7.0) with CUDA & cuDNN support (GPU computing)
- [AMQP-CPP](https://github.com/CopernicaMarketingSoftware/AMQP-CPP)
- [spdlog](https://github.com/gabime/spdlog)
- [Boost 1.82.0](https://www.boost.org/users/history/version_1_82_0.html) 

# Platform
The currently supported platforms are ``POSIX based`` only.
It is due to limitations in AMQP-CPP TcpHandler. (It is possible to support Windows by implementing TcpHandler for Windows)

# Build & Run
```
cd ~
git clone https://github.com/TheYoungBeast/Object-Detection-MicroService-CPP.git
cd Object-Detection-MicroService-CPP
mkdir build && cd build
cmake ..
make
```

## Having a hard time building the project?
If you have any troubles, please refer to the [dockerfile](https://github.com/TheYoungBeast/Object-Detection-MicroService-CPP/blob/main/dockerfile). The whole project and all the dependencies are built step by step. If that still does not help make sure you have NVIDIA GPU, Nvidia Toolkit, and GPU Drivers installed.
Consider installing it using docker if you don't plan to modify the project.

## Run (local)
First of all: **set environment variables**! You can do this by running a prepared shell script. Edit it accordingly to your needs.
```
cd ~/Object-Detection-MicroService-CPP
. env.sh
```
Then launch:
```
cd build
./micro_od 'yolov8n.onnx' 640 640
```
1st param is Yolo's model name. Parameters 2 and 3 are model input shapes (height, width)

# Expected Input & Output (Queues)
Microservice expects messages in JSON format in the ``AVAILABLE_SOURCES`` queue. The minimum required fields are:
```
{
  "id": 2,   <—— the unique numeric source's identifier
  "exchange": "source-feed-2" <—— exchange which belongs to the source
}
```
Source exchanges are expected to be the type of FanOut. Microservice will declare an exchange with the given name in the case where it was not declared yet.
It will allow you to bind other queues and integrate other services e.g. you may share 1 camera device among multiple different services (object detection, face recognition, CCTV, etc.)

Example output: (Single message)
```
[
    {
        "id":62,
        "label":"tv",
        "confidence":80.86636352539062,
        "color":[ 214, 137, 197 ],
        "box":{
            "x":0,
            "y":295,
            "width":162,
            "height":222
        }
    },
    {
        "id":0,
        "label":"person",
        "confidence":70.75984191894531,
        "color":[ 157, 169, 165 ],
        "box":{
            "x":255,
            "y":121,
            "width":224,
            "height":515
        }
    }
]
```
Microservice will create an output exchange for each source. (But now when I think of that I'll probably change it to 1 exchange and use routing keys)
  
# To Do:
- Add ``health checks`` or ``Service registry`` pattern for microservices
- ~~Add logger~~
- Attach timestamps to results 
- Elasticsearch?
- ~~Add docker container~~
- ~~Add JSON/Protobuf Support~~
- Refactor code
- ~~Support at least Yolo V5 & Yolo V8~~

# Contact?
Please find me on: <br />
