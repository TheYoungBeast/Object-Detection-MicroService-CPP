#include "../inc/message_bus_client.hpp"

#include <future>

message_bus_client::message_bus_client(const std::string_view& address, uint threads_hint)
    : m_service(threads_hint), 
    m_handler(m_service), 
    m_connection(&m_handler, AMQP::Address("amqp://localhost/")),
    channel(&m_connection)
{
    thread = std::thread([&](){
        m_service.run();
    });
}

void message_bus_client::thread_join() {
    this->thread.join();
}

message_bus_client& message_bus_client::declare_exchange(const std::string_view& exchange_name, const AMQP::ExchangeType type)
{
    this->channel.declareExchange(exchange_name, type)
        .onSuccess([=](){
            // add spdlog here
            std::cout << "Successfuly declared " << exchange_name << " Exchange" << std::endl;
        })
        .onError([](const char* error){
            // add spdlog here
            std::cerr << error << std::endl;
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
            channel.bindQueue(exchange, name, "");
            std::cout << "Declared queue: " << name << std::endl;
            std::cout << "Binded " << name << " queue with " << exchange << std::endl;

            m_binded_queues[exchange] = name;
            auto& consumer = channel.consume(name);
            consumer.onMessage(callback);
        })
        .onError([](const char* error) {
            std::cerr << "Unable to declare queue - " << error << std::endl;
        });
    
    return *this;
}

message_bus_client& message_bus_client::add_listener(const std::string exchange, AMQP::MessageCallback callback, int flags)
{
    this->channel.declareQueue(flags)
        .onSuccess([&,exchange, callback](const std::string &name, uint32_t messagecount, uint32_t consumercount)
        {
            channel.bindQueue(exchange, name, "");
            std::cout << "Declared queue: " << name << std::endl;
            std::cout << "Binded " << name << " queue with " << exchange << std::endl;

            m_binded_queues[exchange] = name;
            auto& consumer = channel.consume(name);
            consumer.onMessage(callback);
        })
        .onError([](const char* error) {
            std::cerr << "Unable to declare queue - " << error << std::endl;
        });
    
    return *this;
}
