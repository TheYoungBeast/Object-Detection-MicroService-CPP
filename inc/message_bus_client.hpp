#pragma once
#ifndef MESSAGE_BUS_CLIENT_H
#define MESSAGE_BUS_CLIENT_H

#include <string_view>
#include <optional>
#include <thread>
#include <unordered_set>

// Boost
#include <boost/asio/io_service.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/deadline_timer.hpp>

// AMQP RabbitMQ
#include <amqpcpp.h>
#include <amqpcpp/envelope.h>
#include <amqpcpp/libboostasio.h>


class message_bus_client
{
    using client = message_bus_client;
    using client_ref = message_bus_client&;
    
    protected:
        std::thread thread;
        boost::asio::io_service m_service;
        AMQP::LibBoostAsioHandler m_handler;
        AMQP::TcpConnection m_connection;
        AMQP::TcpChannel channel;

        std::unordered_map<std::string, std::string> m_binded_queues = std::unordered_map<std::string, std::string>();

    public:
        /** 
         * Non-blocking message bus client using AMQP for RabbitMQ Server.
         * Multithreaded boost handler (boost::asio::io_service)
         * @note to join client's thread call thread_join() method
         * @note avoid passing any non-owning objects
        */
        message_bus_client(const std::string_view&, uint = 2);

        void thread_join();
        
        std::optional<const std::string> get_binded_queue(const std::string_view& exchange_name);
        client_ref declare_exchange(const std::string_view& exchange_name, const AMQP::ExchangeType exchange_type = AMQP::ExchangeType::direct);
        
        client_ref add_listener(const std::string exchange, const std::string queue, AMQP::MessageCallback callback, int flags = 0);
        client_ref add_listener(const std::string exchange_name, AMQP::MessageCallback callback, int flags = 0);
        
        bool publish(const std::string_view &exchange, const std::string_view &routingKey, const AMQP::Envelope &envelope, int flags = 0);
        bool publish(const std::string_view &exchange, const std::string_view &routingKey, const std::string &message, int flags = 0);

        virtual ~message_bus_client() = default;
};

#endif // MESSAGE_BUS_CLIENT_H