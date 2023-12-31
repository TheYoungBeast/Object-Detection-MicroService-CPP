#include "../inc/publisher/data_publisher.hpp"

data_publisher::data_publisher(std::shared_ptr<rabbitmq_client> client)
    : basic_publisher(client), data_converter( std::unique_ptr<converter>(new json_converter()) )
{
}

data_publisher::data_publisher(std::shared_ptr<rabbitmq_client> client, std::unique_ptr<converter>& custom_converter)
    : basic_publisher(client), data_converter(std::move(custom_converter))
{
}

std::string json_converter::convert(const std::vector<detection>& results)
{
   boost::json::array array;

    for(auto& det: results)
    {
        boost::json::object obj;
        obj["id"] = det.class_id;
        obj["label"] = det.class_name;
        obj["confidence"] = det.confidence*100;
        obj["color"] = { det.color[0], det.color[1], det.color[2] };
        obj["box"] = { {"x", det.box.x}, {"y", det.box.y}, {"width", det.box.width}, {"height", det.box.height} };
        array.emplace_back(obj);
    }
    
    return boost::json::serialize(array);
}

bool data_publisher::publish(unsigned src_id, const std::vector<detection>& result)
{
    if(!is_declared(src_id))
        declare_exchange(src_id, this->prefix);

    auto data = data_converter->convert(result);
    AMQP::Envelope envelope(data);
    
    return rabbitmq->publish(declared_exchanges[src_id], "", envelope );
}

bool data_publisher::publish(unsigned src_id, const std::vector<detection>& results, unsigned limit)
{
    auto shift = limit == 0 ? results.size() : limit;

    //auto end = std::distance(results.begin()+shift, res)

    //auto subvec = std::vector<detection>(results.begin(), results.begin()+shift);

    if(results.empty())
        this->publish(src_id, {});
    else
        this->publish(src_id, results);

    return true;
}