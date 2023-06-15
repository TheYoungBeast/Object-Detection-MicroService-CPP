#pragma once

#ifndef RabbitMQ_CALLBACKS_H
#define RabbitMQ_CALLBACKS_H

#include <string>
#include <sstream>
#include <iostream>
#include <functional>
#include <optional>

#include <amqpcpp.h>

#include <opencv2/opencv.hpp>

#include "detection_service.hpp"

class callback_factory
{
    public:
        static auto create_new_source_callback(AMQP::Channel& channel)
        {
            return [&channel](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered) 
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

                auto& service = detection_service::get_service_instance();
                auto metrics = service.get_performance();
                std::cout << metrics.avgProcessingTime << "ms" << "\t" << metrics.avgFPS << " FPS" << std::endl;

                std::cout << "New source available: " << exchangeName << "\tdevice-id:" << source_id << std::endl;
                service.register_source(source_id);

                channel.declareQueue(AMQP::exclusive | AMQP::autodelete)
                .onSuccess([&channel, exchangeName](const std::string &name, uint32_t messagecount, uint32_t consumercount){
                    channel.bindQueue(exchangeName, name, "");

                    auto& consumer = channel.consume(name);
                    AMQP::MessageCallback callback = callback_factory::create_new_frame_delivered_callback(channel);
                    consumer.onMessage(callback);
                });

                channel.ack(deliveryTag);
                return;
            };
        }

        static auto create_unregister_source_callback(AMQP::Channel& channel)
        {
            return [&](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered) 
            {
                std::string_view exchangeName = message.body();
            
                const std::string header = "device-id";
                auto& field = message.headers().get(header);

                if(!field.isInteger())
                {
                    std::cerr << "Header (" + header + ") field invalid type" << std::endl;
                    return;
                }
                
                int device_id = int(field);

                if(!detection_service::get_service_instance().unregister_source(device_id))
                    return;

                throw std::runtime_error("Not fully implemented yet");
                std::cout << "Unregistered exchange: " << exchangeName << std::endl;

                //channel.unbindQueue(exchangeName, queueName.value(), "");

                //std::cout << "Unbinded queueu: " << queueName.value() << " from exchange: " << exchangeName << std::endl;
                channel.ack(deliveryTag);
                return;
            };
        }

        static AMQP::MessageCallback create_new_frame_delivered_callback(AMQP::Channel& channel)
        {
            return [&channel](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered) 
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

                cv::Mat rawData(1, message.bodySize(), CV_8SC1, (void*)message.body());
                cv::Mat decodedMat = cv::imdecode(rawData, cv::IMREAD_COLOR);
                cv::Mat* ptr = new cv::Mat(decodedMat);

                auto& service = detection_service::get_service_instance();
                
                if(service.try_add_to_queue(source_id, ptr))
                    channel.ack(deliveryTag);
                    
                return;
            };
        }
};


#endif // RabbitMQ_CALLBACKS_H