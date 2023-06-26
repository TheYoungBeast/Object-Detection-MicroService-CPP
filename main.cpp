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
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/algorithm/string.hpp>

// AMQP RabbitMQ
#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>

// spdlog
#include "spdlog/spdlog.h"

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
#include "inc/defaults.hpp"

using namespace std;
using namespace cv;
using namespace dnn;
using namespace cuda;


int main(int argc, const char** argv)
{
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] [thread %t] %v"); // %z for time zone
    spdlog::flush_on(spdlog::level::debug);
    spdlog::set_level(spdlog::level::debug);

    boost::program_options::options_description desc("Options");

    desc.add_options()
        ("type", boost::program_options::value<std::string>()->default_value("v8"), "AI model type e.g. v8, v5. Default: v8")
        ("shape", boost::program_options::value<std::string>()->default_value("640x640"), "model shape (Width x Height) e.g. 640x640. Default: 640x640")
        ("path", boost::program_options::value<std::string>(), "path to resources (models)")
        ("model", boost::program_options::value<std::string>()->default_value("yolov8n.onnx"), "model name e.g. yolov8n.onnx. Default: yolov8n.onnx");

    desc.print(std::cout);

    boost::program_options::variables_map vm;

    try {
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
    } catch (const boost::program_options::unknown_option& u) {
        spdlog::warn(u.what());
    }
    
    boost::program_options::notify(vm);

    std::string type;
    if(!vm.count("type")) {
        const std::string env = "model_type";
        spdlog::critical("'type' parameter is not specified");
        spdlog::info("Looking for {} env variable...", env);

        get_env_or_throw(env); 
    } else type = vm["type"].as<std::string>();

    std::string shape;
    if(!vm.count("shape")) {
        const std::string env = "model_shape";
        spdlog::critical("'shape' parameter is not specified");
        spdlog::info("Looking for {} env variable...", env);

        shape = get_env_or_throw(env); 
    } else shape = vm["shape"].as<std::string>();
    
    
    auto pos = shape.find("x");

    if(pos == std::string::npos) {
        spdlog::critical("Invalid model shape {}'", shape);
        return -1;
    }

    int width = std::atoi(shape.substr(0, pos).c_str());
    int height = std::atoi(shape.substr(pos+1, shape.length()).c_str());

    if(width <= 0 || height <= 0) {
        spdlog::critical("Invalid model shape {}x{}", width, height);
        return -1;
    }

    cv::Size size = cv::Size(width, height);

    std::string path;
    if(!vm.count("path")) {
        const std::string env = "model_path";
        spdlog::critical("'path' parameter is not specified");
        spdlog::info("Looking for {} env variable...", env);

        get_env_or_throw(env); 
    } else path = vm["path"].as<std::string>();

    std::string model;
    if(!vm.count("model")) {
        const std::string env = "model_name";
        spdlog::critical("'model' parameter is not specified");
        spdlog::info("Looking for {} env variable...", env);

        get_env_or_throw(env); 
    } else model = vm["model"].as<std::string>();

    spdlog::info("Using spdlog version {}.{}.{}!", SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH);
    spdlog::info("Using OpenCV version {}", CV_VERSION);
    
    const std::string modelsPath = path;
    const auto& model_name = model;
    const cv::Size model_shape = size;

    #pragma region ENV

    const std::string_view amqp_host_env = "RabbitMQ_Address";
    const std::string_view available_sources_env = "EXCHANGE_AVAILABLE_SOURCES";
    const std::string_view unregister_sources_env = "EXCHANGE_UNREGISTER_SOURCES";

    const std::string_view available_sources_que_env = "AVAILABLE_SOURCES_QUEUE";
    const std::string_view unregister_sources_que_env = "UNREGISTER_SOURCES_QUEUE";

    const auto& amqp = get_environment_variable(amqp_host_env);
    auto const& amqp_host = amqp.has_value() ? amqp.value() : DEFAULT::AMQP_HOST;

    const auto& as_exchange = get_environment_variable(available_sources_env);
    auto const& available_sources_exchange = as_exchange.has_value() ? as_exchange.value() : DEFAULT::AVAILABLE_SOURCES_EX;

    const auto& os_exchange = get_environment_variable(unregister_sources_env);
    auto const& unregister_sources_exchange = os_exchange.has_value() ? os_exchange.value() : DEFAULT::OBSOLETE_SOURCES_EX;

    const auto& as_que = get_environment_variable(available_sources_que_env);
    auto const& available_sources_que = as_que.has_value() ? as_que.value() : DEFAULT::AVAILABLE_SOURCES_QUE;

    const auto& os_que =  get_environment_variable(unregister_sources_que_env);
    auto const& unregister_sources_que = os_que.has_value() ? os_que.value() : DEFAULT::OBSOLETE_SOURCES_QUE;

    #pragma endregion

    std::vector<std::thread> background_services;

    #pragma region YOLO

    const std::chrono::seconds gpu_warm_up_time(5);

    std::unique_ptr<detection_model> model_ptr;
    
    if(boost::iequals(type, "v5")) {
        model_ptr = std::make_unique<yolo_v5>(model_shape, modelsPath, model_name);
        spdlog::info("Creating model v5");
    }
    else {
        model_ptr = std::make_unique<yolo_v8>(model_shape, modelsPath, model_name);
        spdlog::info("Creating model v8");
    }

    auto& service = detection_service::get_service_instance();
    service.use_model(model_ptr);

    background_services.emplace_back(service.run_background_service());

    spdlog::info("Warming up GPU...");
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

    std::vector<std::thread> rabbitmq_clients;

    rabbitmq_clients.emplace_back( rabbitmq->client_run() );
    rabbitmq_clients.emplace_back( rabbitmq_publisher->client_run() );

    for(auto& client: rabbitmq_clients)
        client.join();

    #pragma endregion RABBITMQ

    for(auto& service: background_services)
        service.join();

    return 0;
}