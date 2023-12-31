#include "../inc/rabbitmq/message_bus_client.hpp"
#include <future>

message_bus_client::message_bus_client(const std::string_view& host, const std::chrono::duration<int>& reconnect_it)
    : reconnect_interval( reconnect_it ),
    address( AMQP::Address(host.begin()) ),
    evbase( event_base_new() ), 
    m_handler( evbase.get() )
    //m_connection( new AMQP::TcpConnection(&m_handler, address ) ),
    //channel( std::make_unique<AMQP::TcpChannel> (m_connection.get()) )
{
    this->connection_init();
    this->channel_init();
}

void message_bus_client::connection_init()
{
    if(m_connection)
        m_connection->close();

    m_connection = std::make_unique<AMQP::TcpConnection>(&m_handler, address);

    this->channel_init();
}

void message_bus_client::channel_init()
{
    if(!m_connection || m_connection->closed())
        return;

    if(channel && channel->usable())
        return;

    channel = std::make_unique<AMQP::TcpChannel>( m_connection.get() );

    channel->onError([this](const char* message) {
        spdlog::error("AMQP channel error: {}", message);
        this->isChannelInErrorState = true;
        this->isChannelReady = false;
    });

    channel->onReady([this](){
        spdlog::info("Channel ready");
        this->isChannelReady = true;

        while(channel && isChannelInErrorState)
            this->restore();
    });
}

void message_bus_client::restore() 
{
    if(!isChannelInErrorState || !isChannelReady)
        return;

    auto actions_to_restore = actions.get_all();
    actions.clear();
    
    auto size2 = actions_to_restore.size();
    auto size = actions.size();

    m_binded_queues.clear();

    for(auto& action: actions_to_restore) {
        action(); // invoke action
    }

    if(channel->usable() && isChannelReady)
        isChannelInErrorState = false;
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
        // 0 Success
        // 1 no events pending/active
        while(ret != -1)
        {
            ret = event_base_dispatch( evbase.get() );

            if(isChannelInErrorState)
            {
                this->connection_init();
                spdlog::warn("Connection failed. Restoring connectiong...");
                std::this_thread::sleep_for(reconnect_interval);
            }
        }

        return ret;
    });
}

message_bus_client& message_bus_client::declare_exchange(const std::string& exchange_name, const AMQP::ExchangeType type)
{
    auto context = std::bind(&message_bus_client::declare_exchange, this, exchange_name, type);
    std::function action = [=](){ context(); };

    actions.push(action);

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
    auto context = std::bind(
        static_cast < message_bus_client&  (message_bus_client::*) (const std::string, const std::string, AMQP::MessageCallback, int) > // cast
        (&message_bus_client::add_listener),  // method ptr
        this, exchange, queue, callback, flags); 

    std::function action = [=](){ context(); };

    actions.push(action);

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
    auto context = std::bind(
        static_cast < message_bus_client&  (message_bus_client::*) (const std::string, AMQP::MessageCallback, int) > // cast
        (&message_bus_client::add_listener),  // method ptr
        this, exchange, callback, flags); 

    std::function action = [=](){ context(); };

    actions.push(action);

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
