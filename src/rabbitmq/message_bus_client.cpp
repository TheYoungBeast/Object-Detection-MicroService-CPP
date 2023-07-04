#include "../inc/rabbitmq/message_bus_client.hpp"
#include <future>

message_bus_client::message_bus_client(const std::string_view& host)
    : address( AMQP::Address(host.begin()) ),
    evbase( event_base_new() ), 
    m_handler(evbase),
    m_connection( new AMQP::TcpConnection(&m_handler, address ) ),
    channel( new AMQP::TcpChannel(m_connection) )
{
    channel->onError([this](const char* message) {
        spdlog::error("AMQP channel error: {}", message);
        // Recover channel here!
        this->valid = false;
    });
}

message_bus_client::~message_bus_client() {
    event_base_free(evbase);
    delete channel;
    delete m_connection;
}

void message_bus_client::connection_init()
{
    if(m_connection && !m_connection->closed())
        m_connection->close();

    //m_connection = std::make_unique<AMQP::TcpConnection>(&m_handler, address );
}

void message_bus_client::channel_init()
{
    if(!m_connection || m_connection->closed())
        this->connection_init();

    if(channel && channel->usable())
        channel->close();

    //channel = std::make_unique<AMQP::TcpChannel>(m_connection.get());

    channel->onError([this](const char* message) {
        spdlog::error("AMQP channel error: {}", message);
        // Recover channel here!
    });
}

std::thread message_bus_client::client_run() 
{
    if(started)
        throw std::runtime_error("client_run() was called twice for the same client instance");

    started = true;

    return std::thread([&]()
    {
        int ret = 0;
        // -1 means error occured
        while(ret != -1)
        {
            ret = event_base_dispatch(evbase);
            //spdlog::info("Event dispatcher returned: {}", ret);
            std::this_thread::yield();
        }
    });
}

message_bus_client& message_bus_client::declare_exchange(const std::string& exchange_name, const AMQP::ExchangeType type)
{
    this->channel->declareExchange(exchange_name, type)
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
    this->channel->declareQueue(queue, flags)
        .onSuccess([&,exchange, callback](const std::string &name, uint32_t messagecount, uint32_t consumercount)
        {
            spdlog::info("Successfully declared {} queue", name);

            channel->bindQueue(exchange, name, "").onSuccess([=](){
                spdlog::info("Successfully binded {} with {}", name, exchange);
            }).onError([](const char* msg) {
                spdlog::error(msg);
            });

            m_binded_queues[exchange] = name;
            auto& consumer = channel->consume(name);
            consumer.onMessage(callback);
        })
        .onError([=](const char* error) {
            spdlog::error("Error declaring queue {}: {}", queue, error);
        });
    
    return *this;
}

message_bus_client& message_bus_client::add_listener(const std::string exchange, AMQP::MessageCallback callback, int flags)
{
    this->channel->declareQueue(flags)
        .onSuccess([&,exchange, callback](const std::string &name, uint32_t messagecount, uint32_t consumercount)
        {
            spdlog::info("Successfully declared {} queue", name);

            channel->bindQueue(exchange, name, "").onSuccess([=](){
                spdlog::info("Successfully binded {} with {}", name, exchange);
            }).onError([](const char* msg) {
                spdlog::error(msg);
            });

            m_binded_queues[exchange] = name;
            auto& consumer = channel->consume(name);
            consumer.onMessage(callback);
        })
        .onError([=](const char* error) {
            spdlog::error("Error declaring queue : {}", error);
        });
    
    return *this;
}

bool message_bus_client::publish(const std::string_view &exchange, const std::string_view &routingKey, const AMQP::Envelope &envelope, int flags)
{
    if(channel->usable())
        return this->channel->publish(exchange, routingKey, envelope, flags);
    else
        spdlog::warn("Channel unusable");

    return false;
}
        
bool message_bus_client::publish(const std::string_view &exchange, const std::string_view &routingKey, const std::string &message, int flags)
{
    if(channel->usable())
        return this->publish(exchange, routingKey, AMQP::Envelope(message.data(), message.size()), flags);
    else
        spdlog::warn("Channel unusable");

    return false;
}
