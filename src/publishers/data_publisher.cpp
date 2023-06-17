#include "../../inc/data_publisher.hpp"

data_publisher::data_publisher(std::shared_ptr<rabbitmq_client> client)
    : rabbitmq(client)
{
}

void data_publisher::declare_exchange(unsigned src_id)
{
    auto exchange = this->prefix + boost::lexical_cast<std::string>(src_id);
    rabbitmq->declare_exchange(exchange, AMQP::ExchangeType::fanout);
    this->declared_exchanges.try_emplace(src_id, exchange);
}

bool data_publisher::publish(unsigned src_id, detection result)
{
    if(!is_declared(src_id))
        declare_exchange(src_id);

    return rabbitmq->publish(declared_exchanges[src_id], "", to_json(result));
}

std::string data_publisher::to_json(detection d)
{
    boost::json::object obj;
    boost::json::serialize(obj);
    obj["id"] = d.class_id;
    obj["label"] = d.class_name;
    obj["confidence"] = d.confidence;
    obj["color"] = { d.color[0], d.color[1], d.color[2] };
    obj["box"] = { {"x", d.box.x}, {"y", d.box.y}, {"width", d.box.width}, {"height", d.box.height} };
    
    return boost::json::serialize(obj);
}

bool data_publisher::is_declared(unsigned src_id)
{
    return this->declared_exchanges.find(src_id) != this->declared_exchanges.end();
}

bool data_publisher::publish(unsigned src_id, const std::vector<detection> results, unsigned limit)
{
    auto threshold = limit == 0 ? std::numeric_limits<unsigned>::max() : limit;
    auto counter = 0;

    for(auto& result: results)
    {
        if(counter == threshold)
            break;

        this->publish(src_id, result);
        ++counter;
    }

    return true;
}