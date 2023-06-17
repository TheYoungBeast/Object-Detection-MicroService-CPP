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
    private:
        std::shared_ptr<rabbitmq_client> rabbitmq;
        std::string prefix = "detection-results-";
        std::map<unsigned, std::string> declared_exchanges;

    public:
        data_publisher(std::shared_ptr<rabbitmq_client> client);
        virtual ~data_publisher() = default;

        std::string to_json(detection d);

        bool publish(unsigned src_id, detection d);
        bool publish(unsigned src_id, const std::vector<detection> results, unsigned limit = 0);

    private:
        void declare_exchange(unsigned src_id);
        bool is_declared(unsigned src_id);
};

#endif // DATA_PUBLISHER_H