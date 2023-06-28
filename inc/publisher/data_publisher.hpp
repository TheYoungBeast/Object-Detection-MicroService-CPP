#pragma once

#ifndef DATA_PUBLISHER_H
#define DATA_PUBLISHER_H

#include <limits>
#include <memory>
#include <sstream>

#include <boost/json.hpp>

#include "basic_publisher.hpp"

/**
 * @note Join Rabbitmq client before injecting it to the class
 * @note Do not share rabbitmq client among threads 
*/
class data_publisher : public basic_publisher
{
    public:
        class converter
        {
            public:
                virtual ~converter() = default;
                /**
                 * @brief Converts detections list into another data format
                */
                virtual std::string convert(const std::vector<detection>& results) = 0;
        };

    protected:
        std::unique_ptr<converter> data_converter;
        std::string prefix = "detection-results-";

    public:
        data_publisher(std::shared_ptr<rabbitmq_client> client);
        data_publisher(std::shared_ptr<rabbitmq_client> client, std::unique_ptr<converter>& data_converter);
        virtual ~data_publisher() = default;

        bool publish(unsigned src_id, const std::vector<detection>& results);
        bool publish(unsigned src_id, const std::vector<detection>& results, unsigned limit);
};

/**
 * @brief DATA => JSON Converter
*/
class json_converter : public data_publisher::converter
{
    public:
        json_converter() = default;
        virtual ~json_converter() = default;

        /**
         * @brief Converts detections list into JSON data
         * @returns JSON
        */
        virtual std::string convert(const std::vector<detection>& results) override;
};

#endif // DATA_PUBLISHER_H