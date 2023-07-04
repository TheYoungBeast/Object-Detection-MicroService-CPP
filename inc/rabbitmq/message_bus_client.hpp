#pragma once
#ifndef MESSAGE_BUS_CLIENT_H
#define MESSAGE_BUS_CLIENT_H

#include <string_view>
#include <optional>
#include <thread>
#include <unordered_set>
#include <memory>

// AMQP RabbitMQ
#include <event2/event.h>
#include <amqpcpp.h>
#include <amqpcpp/libevent.h>

// spdlog
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

class RabbitMqHandler : public AMQP::LibEventHandler
{
    public:
        RabbitMqHandler(struct event_base* evbase) : LibEventHandler(evbase), evbase_(evbase_) {}

        virtual void onHeartbeat(AMQP::TcpConnection *connection) {
            connection->heartbeat();
            spdlog::debug("heartbeat");
        }

    private:
        struct event_base* evbase_ {nullptr};
};


/**
 * @warning Do not share among threads. Connection and channel are not thread-safe because of the implementation of AMQP-CPP
 * @note see https://github.com/CopernicaMarketingSoftware/AMQP-CPP/issues/92
*/
class message_bus_client
{
    using client = message_bus_client;
    using client_ref = message_bus_client&;
    
    private:
        bool started = false;
        bool valid = true;
        event_base* evbase;
        AMQP::Address address;
        RabbitMqHandler m_handler;

    protected:
        AMQP::TcpConnection* m_connection;
        AMQP::TcpChannel* channel;

        std::unordered_map<std::string, std::string> m_binded_queues = std::unordered_map<std::string, std::string>();

    public:
        /** 
         * @param host e.g. "amqp://localhost"
         * @note not thread-safe
         * @note avoid passing any non-owning objects
        */
        message_bus_client(const std::string_view& host);

        std::thread client_run();
        
        std::optional<const std::string> get_binded_queue(const std::string_view& exchange_name);
        client_ref declare_exchange(const std::string& exchange_name, const AMQP::ExchangeType exchange_type = AMQP::ExchangeType::direct);
        
        client_ref add_listener(const std::string exchange, const std::string queue, AMQP::MessageCallback callback, int flags = 0);
        client_ref add_listener(const std::string exchange_name, AMQP::MessageCallback callback, int flags = 0);
        
        bool publish(const std::string_view &exchange, const std::string_view &routingKey, const AMQP::Envelope &envelope, int flags = 0);
        bool publish(const std::string_view &exchange, const std::string_view &routingKey, const std::string &message, int flags = 0);

        virtual ~message_bus_client();

    private:
        void connection_init();
        void channel_init();
};

#endif // MESSAGE_BUS_CLIENT_H