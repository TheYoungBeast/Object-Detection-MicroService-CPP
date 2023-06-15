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
#include "inc/inference.h"
#include "inc/message_bus_client.hpp"
#include "inc/missing_environment_variable.hpp"
#include "inc/utils.hpp"
#include "inc/yolo_v8.hpp"
#include "inc/yolo_v5.hpp"

#include <amqpcpp/flags.h>

using namespace std;
using namespace cv;
using namespace dnn;
using namespace cuda;

// SaS Singleton as Service
template <typename T = cv::Mat>
class basic_detection_service
{
    using class_ref = basic_detection_service<T>&;

    private:
        const std::chrono::seconds sleep_on_empty{1};
        const unsigned max_size_per_que = 30;
        std::map<unsigned, std::queue<T>> queues{};
        std::map<unsigned, std::mutex> que_mutexes{};
        std::map<unsigned, float> inference_performance{};
        std::map<unsigned, long long> dropped_frames{};
        unsigned long long total_dropped_frames{0};

        std::mutex inference_mutex{};
        std::unique_ptr<detection_model> model;

    protected:
        basic_detection_service() = default;
        basic_detection_service(const basic_detection_service<T>&) = delete;
        basic_detection_service(basic_detection_service<T>&&) = delete;
        void operator=(const basic_detection_service<T>&) = delete;
        void operator=(basic_detection_service<T>&&) = delete;

    public:
        static class_ref get_service_instance() {
            static basic_detection_service<T> service{}; // lazy init
            return service;
        }

        void use_model(std::unique_ptr<detection_model>& ptr) {
            model = std::move(ptr);
        }

        void use_model(detection_model&& t_model) {
            model = std::make_unique<detection_model>(std::move(t_model));
        }

        bool register_source(const unsigned source_id) {
            if(!contains(source_id))
                return false;

            queues.try_emplace(source_id);
            que_mutexes.try_emplace(source_id);
            dropped_frames.try_emplace(source_id);

            //assert(queues.size() == que_mutexes.size() == dropped_frames.size());

            std::cout << "[DetectionService]: Registered new source:" << source_id << std::endl;
            return true;
        }

        bool unregister_source(const unsigned source_id) 
        {
            if(!contains(source_id))
                return false;

            std::lock_guard lock(inference_mutex); // lock inference then remove all sources
            
            queues.erase(source_id);
            que_mutexes.erase(source_id);
            dropped_frames.erase(source_id);
            
            return true;
        }

        bool add_to_queue(const unsigned source_id, T& frame) {
            // if(not contains(source_id))
            //     register_source(source_id);

            std::lock_guard<std::mutex> lock(que_mutexes[source_id]);

            if(queues[source_id].size() >= max_size_per_que)
            {
                queues[source_id].pop(); // remove oldest pending frame, add new one
                ++total_dropped_frames;
                dropped_frames[source_id] = ++dropped_frames[source_id];
            }
            
            queues[source_id].emplace(frame);

            return true;
        }

        std::thread run_background_service() {
            return std::thread([&](){
                std::cout << "Run background service" << std::endl; 
                this->inference(); });
        }

    private:
        void inference()
        {
            if(model.get() == nullptr)
                throw std::runtime_error("Set detection model before running inference");

            int current_que_index = -1;

            while (true)
            {
                std::lock_guard lock(inference_mutex);

                if(queues.empty())
                { 
                    std::cout << "No pending queues, sleeping..." <<std::endl;
                    std::this_thread::sleep_for(sleep_on_empty);
                    continue;
                }

                current_que_index = (current_que_index+1)%queues.size();

                if(queues[current_que_index].empty())
                    continue;

                std::cout << "Queue: " << current_que_index 
                << "\tPending: " << queues[current_que_index].size() 
                << "\tDropped: " << dropped_frames[current_que_index]
                << "\tTotal Dropped: " << total_dropped_frames
                << std::endl;

                std::unique_lock que_lock(que_mutexes[current_que_index]); // lock queue
                
                auto frame = queues[current_que_index].front();
                queues[current_que_index].pop();

                que_lock.unlock(); // unlock queue to write;

                auto&& output = model->object_detection(frame);
                std::cout << output.size() << " detections" << std::endl;
                model->apply_detections_on_image(frame, output);
                cv::imwrite("view"+std::to_string(current_que_index)+".jpg", frame);
            }
        }

        bool contains(int source_id) {
            return queues.find(source_id) != this->queues.end();
        }
};


typedef basic_detection_service<> detection_service;


int main(int argc, const char** argv)
{
    std::cout << "Usage: ./opencvtest 'model' height width" << std::endl;

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

    #pragma region YOLO

    const std::chrono::seconds gpu_warm_up_time(5);

    std::unique_ptr<detection_model> model_ptr(new yolo_v5(model_shape, modelsPath, model_name));

    auto& service = detection_service::get_service_instance();
    service.use_model(model_ptr);
    auto background_service = service.run_background_service();

    std::cout << "Starting GPU..." << std::endl;
    service.register_source(0);
    auto warm_up_frame = cv::Mat(cv::Mat::zeros(model_shape, CV_8UC3));
    service.add_to_queue(0, warm_up_frame);

    // sleep this thread
    // let background service warm up the GPU cache and connection
    // so it won't clouge up queues with pending frames from rabbitmq client
    std::this_thread::sleep_for(gpu_warm_up_time);
    service.unregister_source(0);

    #pragma endregion YOLO

    auto bus_client = message_bus_client(amqp_host);
    auto& channel = bus_client.get_channel();

    auto imageFrameDeliveredCallback = [&channel](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered) 
    {
        const std::string header = "device-id";

        auto& field = message.headers().get(header);
        if(!field.isInteger())
        {
            std::cerr << "Header (" + header + ") field invalid type" << std::endl;
            channel.ack(deliveryTag); // acknowledge anyway
            return;
        }

        auto source_id = int(field);

        Mat rawData(1, message.bodySize(), CV_8SC1, (void*)message.body());
        Mat decodedMat = imdecode(rawData, IMREAD_COLOR);

        detection_service::get_service_instance().add_to_queue(source_id, decodedMat);
        channel.ack(deliveryTag);
    };
    
    auto newSourceAvailableCallback = [&](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered) 
    {
        std::string exchangeName = std::string(message.body(), message.bodySize());
        std::stringstream s{};

        const std::string header = "device-id";

        auto& field = message.headers().get(header);
        if(!field.isInteger())
        {
            std::cerr << "Header (" + header + ") field invalid type" << std::endl;
            channel.ack(deliveryTag); // acknowledge anyway
            return;
        }

        auto source_id = int(field);

        std::cout << "New source available: " << exchangeName << "\tdevice-id:" << source_id << std::endl;
        detection_service::get_service_instance().register_source(source_id);

        bus_client.add_listener(exchangeName, imageFrameDeliveredCallback, AMQP::exclusive | AMQP::autodelete);
        channel.ack(deliveryTag);
    };

    auto unregisterSourceCallback = [&](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered) 
    {
        channel.ack(deliveryTag);
        std::string_view exchangeName = message.body();
        const auto& queueName = bus_client.get_binded_queue(exchangeName);

        if(!queueName.has_value())
            return;

        throw std::runtime_error("Unregister source is not implemented yet");

        //queue_sources::get_instance().remove_source(exchangeName);
        std::cout << "Unregistered exchange: " << exchangeName << std::endl;


        channel.unbindQueue(exchangeName, queueName.value(), "");
        std::cout << "Unbinded queueu: " << queueName.value() << " from exchange: " << exchangeName << std::endl;
    };

    bus_client.declare_exchange(available_sources_exchange, AMQP::ExchangeType::fanout)
        .declare_exchange(unregister_sources_exchange, AMQP::ExchangeType::fanout);

    bus_client.add_listener(available_sources_exchange, available_sources_que, newSourceAvailableCallback)
        .add_listener(unregister_sources_exchange, unregister_sources_que, unregisterSourceCallback);

    bus_client.thread_join();
    background_service.join();

    return 0;
}