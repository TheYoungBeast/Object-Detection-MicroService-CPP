#pragma once

#ifndef DATA_PUBLISHER_H
#define DATA_PUBLISHER_H

#include <limits>
#include <memory>
#include <sstream>

#include <boost/json.hpp>
#include <boost/lexical_cast.hpp>

#include "rabbitmq_client.hpp"

class data_publisher
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
        std::shared_ptr<rabbitmq_client> rabbitmq;
        std::string prefix = "detection-results-";
        std::map<unsigned, std::string> declared_exchanges;

    public:
        data_publisher(std::shared_ptr<rabbitmq_client> client);
        data_publisher(std::shared_ptr<rabbitmq_client> client, std::unique_ptr<converter>& data_converter);
        virtual ~data_publisher() = default;

        bool publish(unsigned src_id, const std::vector<detection>& results);
        bool publish(unsigned src_id, const std::vector<detection>& results, unsigned limit);

    private:
        /**
         * @brief Declares exchange in which the client is going to publish
         * @param src_id source id - creates exchange for this id
        */
        void declare_exchange(unsigned src_id);
        /**
         * @brief Checks wheather this instance already declared exchange for given src_id
         * @param src_id source id
        */
        bool is_declared(unsigned src_id);
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