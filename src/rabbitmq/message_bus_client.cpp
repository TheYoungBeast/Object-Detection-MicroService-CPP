#include "../inc/message_bus_client.hpp"

#include <future>

message_bus_client::message_bus_client(const std::string_view& address, uint threads_hint)
    : m_service(threads_hint), 
    m_handler(m_service), 
    m_connection(&m_handler, AMQP::Address(address.begin())),
    channel(&m_connection)
{
    channel.onError([this](const char* message) {
        spdlog::error("AMQP channel error: {}", message);
        channel.resume(); // ?
        m_service.reset(); // ?
    });
}

std::thread message_bus_client::client_run() 
{
    if(runs > 0)
        throw std::runtime_error("Client_run() was called twice for the same client instance");

    return std::thread([&](){
        m_service.run();
    });
}

message_bus_client& message_bus_client::declare_exchange(const std::string& exchange_name, const AMQP::ExchangeType type)
{
    this->channel.declareExchange(exchange_name, type)
        .onSuccess([=](){
            spdlog::info("Successfully declared {} exchange", exchange_name);
        })
        .onError([=](const char* error){
            spdlog::error("Error declaring exchange {}: {}", exchange_name, error);
        });

    return *this;
}

std::optional<const std::string> message_bus_client::get_binded_queue(const std::string_view& exchange_name)
{
    std::unordered_map<std::string, std::string>::const_iterator it = this->m_binded_queues.find(exchange_name.begin());

    if(it == this->m_binded_queues.end())
        return std::nullopt;

    return it->second;
}
message_bus_client& message_bus_client::add_listener(const std::string exchange, const std::string queue, AMQP::MessageCallback callback, int flags)
{
    this->channel.declareQueue(queue, flags)
        .onSuccess([&,exchange, callback](const std::string &name, uint32_t messagecount, uint32_t consumercount)
        {
            spdlog::info("Successfully declared {} queue", name);

            channel.bindQueue(exchange, name, "").onSuccess([=](){
                spdlog::info("Successfully binded {} with {}", name, exchange);
            }).onError([](const char* msg) {
                spdlog::error(msg);
            });

            m_binded_queues[exchange] = name;
            auto& consumer = channel.consume(name);
            consumer.onMessage(callback);
        })
        .onError([=](const char* error) {
            spdlog::error("Error declaring queue {}: {}", queue, error);
        });
    
    return *this;
}

message_bus_client& message_bus_client::add_listener(const std::string exchange, AMQP::MessageCallback callback, int flags)
{
    this->channel.declareQueue(flags)
        .onSuccess([&,exchange, callback](const std::string &name, uint32_t messagecount, uint32_t consumercount)
        {
            spdlog::info("Successfully declared {} queue", name);

            channel.bindQueue(exchange, name, "").onSuccess([=](){
                spdlog::info("Successfully binded {} with {}", name, exchange);
            }).onError([](const char* msg) {
                spdlog::error(msg);
            });

            m_binded_queues[exchange] = name;
            auto& consumer = channel.consume(name);
            consumer.onMessage(callback);
        })
        .onError([=](const char* error) {
            spdlog::error("Error declaring queue : {}", error);
        });
    
    return *this;
}

bool message_bus_client::publish(const std::string_view &exchange, const std::string_view &routingKey, const AMQP::Envelope &envelope, int flags)
{
    return this->channel.publish(exchange, routingKey, envelope, flags);
}
        
bool message_bus_client::publish(const std::string_view &exchange, const std::string_view &routingKey, const std::string &message, int flags)
{
    return this->publish(exchange, routingKey, AMQP::Envelope(message.data(), message.size()), flags);
}
