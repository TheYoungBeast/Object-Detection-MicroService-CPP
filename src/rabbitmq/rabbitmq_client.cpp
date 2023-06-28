#include "../inc/rabbitmq/rabbitmq_client.hpp"
#include <spdlog/spdlog.h>

rabbitmq_client::rabbitmq_client(const std::string_view& connection_string) : message_bus_client(connection_string)
{
}

rabbitmq_client::rabbitmq_client(const std::string& av_que, const std::string& obsolete_que, const std::string_view& connection_string) 
    : new_source_que_name(av_que), obsolete_source_que_name(obsolete_que), message_bus_client(connection_string)
{
}


rabbitmq_client& rabbitmq_client::init_exchanges(const std::vector<std::pair<std::string, AMQP::ExchangeType>>& exchanges)
{
    for(auto& exchange: exchanges)
        this->declare_exchange(exchange.first, exchange.second);

    return *this;
}

rabbitmq_client& rabbitmq_client::bind_available_sources(const std::string& exchange, detection_service_visitor<cv::Mat>* visitor)
{
    auto callback = this->available_src_msg_callback(visitor);
    this->add_listener(exchange, new_source_que_name, callback);

    return *this;
}

rabbitmq_client& rabbitmq_client::bind_obsolete_sources(const std::string& exchange, detection_service_visitor<cv::Mat>* visitor)
{
    auto callback = this->obsolete_src_msg_callback(visitor);
    this->add_listener(exchange, obsolete_source_que_name, callback);

    return *this;
}

bool rabbitmq_client::validate_json(boost::property_tree::ptree ptree, source& src)
{
    auto id = ptree.get_child_optional("id");

    if(!id.has_value())
        return false;

    auto exchange = ptree.get_child_optional("exchange");

    if(!exchange.has_value())
        return false;

    try
    {
        src.id = id->get<unsigned>("");
        src.exchange = exchange->get<std::string>("");
    }
    catch (const boost::property_tree::ptree_error& e) {
        spdlog::error("JSON validation error: {}", e.what());
    }

    return true;
}

std::optional<source> rabbitmq_client::source_from_json(std::string json)
{
    std::stringstream ss;
    ss << json;

    boost::property_tree::ptree ptree;
    try
    {
        boost::property_tree::read_json(ss, ptree);
    }
    catch (const boost::property_tree::json_parser_error& e)
    {
        spdlog::error("Error reading JSON: {} at line: {}", e.what(), e.line());
        return std::nullopt;
    }

    source src;
    if(!validate_json(ptree, src))
        return std::nullopt;

    return src;
}

AMQP::MessageCallback rabbitmq_client::available_src_msg_callback(detection_service_visitor<cv::Mat>* visitor) 
{
    static AMQP::MessageCallback callback = [this, visitor](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered)
    {
        auto body = std::string(message.body(), message.bodySize());

        auto src = source_from_json(body);

        if(!src.has_value()){
           channel->ack(deliveryTag);
            return;
        }
        
        if(!visitor->visit_new_src(src->id)) {
           channel->ack(deliveryTag); // acknowledge anyway
            return;
        }

        auto callback = this->new_frame_msg_callback(visitor);
        this->declare_exchange(src->exchange, AMQP::ExchangeType::fanout);
        this->add_listener(src->exchange, callback, AMQP::exclusive | AMQP::autodelete);

       channel->ack(deliveryTag);
        return;
    };
    return callback;
}

AMQP::MessageCallback rabbitmq_client::obsolete_src_msg_callback(detection_service_visitor<cv::Mat>* visitor) 
{
    static AMQP::MessageCallback callback = [this, visitor](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered){
        std::string body = std::string(message.body(), message.bodySize());
            
        auto src = source_from_json(body);

        if(!src.has_value()) {
           channel->ack(deliveryTag);
            return;
        }

        if(!visitor->visit_obsolete_src(src->id))
            return;

        auto que = this->get_binded_queue(src->exchange);

        if(que.has_value())
           channel->unbindQueue(src->exchange, que.value(), ""); 

        detection_service::get_service_instance().unregister_source(src->id);
        spdlog::info("Unregistered source (id:{}) from the service", src->id);
        
       channel->ack(deliveryTag);
        return;
    };

    return callback;
}

AMQP::MessageCallback rabbitmq_client::new_frame_msg_callback(detection_service_visitor<cv::Mat>* visitor)
{
    static AMQP::MessageCallback callback = [this, visitor](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered)
    {
        const std::string header = "srcid";
        auto& field = message.headers().get(header);

        if(!field.isInteger()) {
            spdlog::warn("Invalid or missing {} header. Attach {} header with source-id value to your message", header, header);
           channel->ack(deliveryTag);
            return;
        }

        auto source_id = int(field);

        const std::string type = "imgtype";
        auto& type_header = message.headers().get(type);

        int imgtype = -1;

        if(!type_header.isInteger()) {
            spdlog::warn("Invalid or missing {} header.", type);
        }
        else imgtype = int(type_header);

        int width = 0;
        int height = 0;

        const std::string width_field = "imgwidth";
        auto& width_header = message.headers().get(width_field);

        const std::string height_field = "imgheight";
        auto& height_header = message.headers().get(height_field);

        width = int(width_header);
        height = int(height_header);

        auto decodedMat = new cv::Mat();

        if(width <= 0 || height <= 0 || imgtype <= 0)
        {
            spdlog::warn("Missing headers. Attempting to decode frame");
            cv::Mat rawData(1, message.bodySize(), CV_8SC1, (void*)message.body());
            auto decodedMat = new cv::Mat();
            cv::imdecode(rawData, cv::IMREAD_ANYCOLOR, decodedMat);
            
        }
        else {
            *decodedMat = cv::Mat(height, width, imgtype, reinterpret_cast<void*>( const_cast<char*>(message.body())));
        }


        if(decodedMat->empty())
        {
            spdlog::error("Could not decode image. Frame is empty");
            cv::Mat blank = cv::Mat::zeros(640, 640, CV_8UC3);
            blank.copyTo(*decodedMat);
            blank.release();
        }

        auto& service = detection_service::get_service_instance();

        if(visitor->visit_new_frame(source_id, std::shared_ptr<cv::Mat>(decodedMat)))
           channel->ack(deliveryTag);
        
        return;
    };

    return callback;
}

