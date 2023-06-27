FROM nvidia/cuda:12.1.1-cudnn8-devel-ubuntu22.04 as base_rt_cuda

ARG OPENCV_VERSION=4.7.0
ARG CUDA_ARCH_BIN="5.2 5.3 6.0 6.1 6.2 7.0 7.2 7.5 8.0 8.6"

# Install build tools, build dependencies
RUN apt-get update && apt-get upgrade -y &&\
    apt-get install -y \
	python3-pip \
        build-essential \
        cmake \
        git \
        wget \
        unzip \
        yasm \
        pkg-config \
        libswscale-dev \
        libtbb2 \
        libtbb-dev \
        libjpeg-dev \
        libpng-dev \
        libtiff-dev \
        libavformat-dev \
        libpq-dev \
        libxine2-dev \
        libglew-dev \
        libtiff5-dev \
        zlib1g-dev \
        libjpeg-dev \
        libavcodec-dev \
        libavformat-dev \
        libavutil-dev \
        libpostproc-dev \
        libswscale-dev \
        libeigen3-dev \
        libtbb-dev \
        libgtk2.0-dev \
        pkg-config
		
# Install Nvidia Container Toolkit
RUN apt-get install -y nvidia-container-toolkit-base

# Install OpenCV
RUN cd /opt/ && \
    wget https://github.com/opencv/opencv/archive/$OPENCV_VERSION.zip &&\
    unzip $OPENCV_VERSION.zip &&\
    rm $OPENCV_VERSION.zip &&\
    wget https://github.com/opencv/opencv_contrib/archive/$OPENCV_VERSION.zip &&\
    unzip ${OPENCV_VERSION}.zip &&\
    rm ${OPENCV_VERSION}.zip &&\
    mkdir /opt/opencv-${OPENCV_VERSION}/build && cd /opt/opencv-${OPENCV_VERSION}/build &&\
    # Cmake configure
    cmake \
        -DOPENCV_EXTRA_MODULES_PATH=/opt/opencv_contrib-${OPENCV_VERSION}/modules \
        -DWITH_CUDA=ON \
        -DCUDA_ARCH_BIN=${CUDA_ARCH_BIN} \
        -DCUDA_FAST_MATH=ON \
        -DENABLE_FAST_MATH=ON \
        -DCMAKE_BUILD_TYPE=RELEASE \
        -DOPENCV_DNN_CUDA=ON \
        -DBUILD_opencv_dnn=ON \
        -DOPENCV_ENABLE_NONFREE=ON \
        # Install path will be /usr/local/lib (lib is implicit)
        .. && \
	cmake --build . --target=install --config=Release -j 10

# Install AMQP-CPP
RUN cd /opt/ && \
	git clone https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git && \
	cd AMQP-CPP && mkdir build && cd build && \
	cmake .. -DAMQP-CPP_BUILD_SHARED=ON -DAMQP-CPP_LINUX_TCP=ON && \
	cmake --build . --target install

# Install spdlog
RUN apt-get -y install libspdlog-dev

# Install Boost
RUN cd /opt/ && \
    wget https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.tar.gz && \
    tar -xf boost_1_82_0.tar.gz && \
    rm -dr boost_1_82_0.tar.gz && \
    cd boost_1_82_0 && \
    ./bootstrap.sh && \
    ./b2 install

# Install Microservice
RUN mkdir app && cd /app/ && \
    git clone https://github.com/TheYoungBeast/Object-Detection-MicroService-CPP.git && \
    cd Object-Detection-MicroService-CPP && \
    cp env.sh ~/ && \
    mkdir build && cd build && \
    cmake .. && make && \
	cp micro_od /app/ && \
	mkdir /models/ && \
	cp ../assets/models/* /models/

WORKDIR app

# Launch built application
CMD ["./micro_od", "--path", "/models/"]