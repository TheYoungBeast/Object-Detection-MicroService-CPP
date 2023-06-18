#include <iostream>
#include <string_view>
#include <vector>
#include <future>
#include <string>
#include <memory>
#include <optional>
#include <thread>

// OpenCV
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/imgcodecs.hpp>

// Boost
#include <boost/asio/io_service.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/deadline_timer.hpp>

// AMQP RabbitMQ
#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>

// MISC
#include "inc/detection_service.hpp"
#include "inc/rabbitmq_client.hpp"
#include "inc/missing_environment_variable.hpp"
#include "inc/utils.hpp"
#include "inc/yolo_v8.hpp"
#include "inc/yolo_v5.hpp"
#include "inc/background_service.hpp"
#include "inc/processing_service.hpp"
#include "inc/data_publisher.hpp"

using namespace std;
using namespace cv;
using namespace dnn;
using namespace cuda;

int main(int argc, const char** argv)
{
    std::cout << "Usage: ./micro_od 'model' height width" << std::endl;

    if(argc < 4)
        throw std::invalid_argument("Invalid number of arguments");
    
    const std::string assetsPath = "/home/dominik/Dev/OpenCVTest/assets/";
    const std::string modelsPath = assetsPath + "models/";
    const auto& model_name = std::string(argv[1]);
    const cv::Size model_shape = cv::Size2f(std::atoi(argv[2]), std::atoi(argv[3]));

    #pragma region ENV

    const std::string_view amqp_host_env = "RabbitMQ_Address";
    const std::string_view available_sources_env = "EXCHANGE_AVAILABLE_SOURCES";
    const std::string_view unregister_sources_env = "EXCHANGE_UNREGISTER_SOURCES";

    const std::string_view available_sources_que_env = "AVAILABLE_SOURCES_QUEUE";
    const std::string_view unregister_sources_que_env = "UNREGISTER_SOURCES_QUEUE";

    auto const& amqp_host = get_env_or_throw(amqp_host_env);
    auto const& available_sources_exchange = get_env_or_throw(available_sources_env);
    auto const& unregister_sources_exchange = get_env_or_throw(unregister_sources_env);

    auto const& available_sources_que = get_env_or_throw(available_sources_que_env);
    auto const& unregister_sources_que = get_env_or_throw(unregister_sources_que_env);

    #pragma endregion

    std::vector<std::thread> background_services;

    #pragma region YOLO

    const std::chrono::seconds gpu_warm_up_time(5);

    std::unique_ptr<detection_model> model_ptr(new yolo_v8(model_shape, modelsPath, model_name));

    auto& service = detection_service::get_service_instance();
    service.use_model(model_ptr);

    background_services.emplace_back(service.run_background_service());

    std::cout << "Starting GPU..." << std::endl;
    service.register_source(0);
    auto warmup_frame = std::shared_ptr<cv::Mat>(new cv::Mat(cv::Mat::zeros(model_shape, CV_8UC3)));
    service.try_add_to_queue(0, warmup_frame);

    // sleep this thread
    // let background service warm up the GPU cache and connection
    // so it won't clog up queues with pending frames from rabbitmq client
    std::this_thread::sleep_for(gpu_warm_up_time);
    service.unregister_source(0);

    #pragma endregion YOLO

    background_service* bg_processing_service = processing_service::get_service_instance();

    background_services.emplace_back( bg_processing_service->run_background_service() );

    #pragma region RABBITMQ

    auto exchanges = std::vector {
        std::make_pair(available_sources_exchange, AMQP::ExchangeType::fanout),
        std::make_pair(unregister_sources_exchange, AMQP::ExchangeType::fanout)
    };

    auto visitor = &service;
    auto rabbitmq = std::make_shared<rabbitmq_client>(available_sources_que, unregister_sources_que , amqp_host);

    rabbitmq->init_exchanges(exchanges)
        .bind_available_sources(available_sources_exchange, visitor)
        .bind_obsolete_sources(unregister_sources_exchange, visitor);

    auto rabbitmq_publisher = std::make_shared<rabbitmq_client>(amqp_host);

    #pragma region PUBLISHER

    auto publisher = std::make_shared<data_publisher>(rabbitmq_publisher);
    processing_service::get_service_instance()->set_publishers(publisher, nullptr);
    
    #pragma endregion PUBLISHER

    rabbitmq->thread_join();
    rabbitmq_publisher->thread_join();

    #pragma endregion RABBITMQ

    for(auto& service: background_services)
        service.join();

    return 0;
}