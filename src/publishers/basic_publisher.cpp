#include "../inc/publisher/basic_publisher.hpp"

basic_publisher::basic_publisher(std::shared_ptr<rabbitmq_client>& client)
    : rabbitmq( client )
{
}

void basic_publisher::declare_exchange(unsigned src_id, std::string prefix)
{
    auto exchange = prefix + boost::lexical_cast<std::string>(src_id);
    rabbitmq->declare_exchange(exchange, AMQP::ExchangeType::fanout);
    this->declared_exchanges.try_emplace(src_id, exchange);
}

bool basic_publisher::is_declared(unsigned src_id)
{
    return this->declared_exchanges.find(src_id) != this->declared_exchanges.end();
}