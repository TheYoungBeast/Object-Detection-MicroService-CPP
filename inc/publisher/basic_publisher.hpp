#pragma once

#ifndef BASIC_PUBLISHER_HPP
#define BASIC_PUBLISHER_HPP

#include <map>
#include <string>
#include <memory>

#include <boost/lexical_cast.hpp>

#include "../rabbitmq/rabbitmq_client.hpp"

class basic_publisher
{
    protected:
        std::map<unsigned, std::string> declared_exchanges;
        std::shared_ptr<rabbitmq_client> rabbitmq;

    public:
        basic_publisher(std::shared_ptr<rabbitmq_client>& client);
        virtual ~basic_publisher() = default;

    protected:
        /**
         * @brief Declares exchange in which the client is going to publish
         * @param src_id source id - creates exchange for this id
         * @param prefix exchange prefix
        */
        void declare_exchange(unsigned src_id, std::string prefix);
        /**
         * @brief Checks wheather this instance already declared exchange for given src_id
         * @param src_id source id
        */
        bool is_declared(unsigned src_id);
};

#endif // BASIC_PUBLISHER_HPP