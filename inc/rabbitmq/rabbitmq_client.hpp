#pragma once

#ifndef RABBITMQ_CLIENT_H
#define RABBITMQ_CLIENT_H

#include <vector>
#include <string>

#include <boost/property_tree/json_parser.hpp>

#include "message_bus_client.hpp"
#include "../service/detection_service.hpp"

struct source
{
    unsigned id;
    std::string exchange;
};

/**
 * @warning Do not share among threads. Connection and channel are not thread-safe because of the implementation of AMQP-CPP
 * @note see https://github.com/CopernicaMarketingSoftware/AMQP-CPP/issues/92
*/
class rabbitmq_client : public message_bus_client
{
    private:
        const std::string new_source_que_name = "";
        const std::string obsolete_source_que_name = "";

    public:
        rabbitmq_client(const std::string_view& connection_string);
        rabbitmq_client(const std::string& av_que, const std::string& obsolete_que, const std::string_view& connection_string);
        virtual ~rabbitmq_client() = default;

    public:
        rabbitmq_client& init_exchanges(const std::vector<std::pair<std::string, AMQP::ExchangeType>>& exchanges);
        rabbitmq_client& bind_available_sources(const std::string& exchange, detection_service_visitor<cv::Mat>* visitor);
        rabbitmq_client& bind_obsolete_sources(const std::string& exchange, detection_service_visitor<cv::Mat>* visitor);

        bool validate_json(boost::property_tree::ptree ptree, source& src);
        auto source_from_json(std::string s) -> std::optional<source>;

    private:
        AMQP::MessageCallback available_src_msg_callback(detection_service_visitor<cv::Mat>* visitor);
        AMQP::MessageCallback obsolete_src_msg_callback(detection_service_visitor<cv::Mat>* visitor);
        AMQP::MessageCallback new_frame_msg_callback(detection_service_visitor<cv::Mat>* visitor);
};

#endif // RABBITMQ_CLIENT_H