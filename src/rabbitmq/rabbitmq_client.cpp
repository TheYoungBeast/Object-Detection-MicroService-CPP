#include "../../inc/rabbitmq_client.hpp"

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

AMQP::MessageCallback rabbitmq_client::available_src_msg_callback(detection_service_visitor<cv::Mat>* visitor) 
{
    static AMQP::MessageCallback callback = [this, visitor](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered)
    {
        std::string exchangeName = std::string(message.body(), message.bodySize());
        const std::string header = "device-id";

        auto& field = message.headers().get(header);
        if(!field.isInteger())
        {
            std::cerr << "Header (" + header + ") field invalid type" << std::endl;
            channel.ack(deliveryTag); // acknowledge anyway
            return;
        }

        auto source_id = int(field);

        if(!visitor->visit_new_src(source_id)) {
            channel.ack(deliveryTag); // acknowledge anyway
            return;
        }

        auto callback = this->new_frame_msg_callback(visitor);
        this->add_listener(exchangeName, callback, AMQP::exclusive | AMQP::autodelete);

        channel.ack(deliveryTag);
        return;
    };
    return callback;
}

AMQP::MessageCallback rabbitmq_client::obsolete_src_msg_callback(detection_service_visitor<cv::Mat>* visitor) 
{
    static AMQP::MessageCallback callback = [this, visitor](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered){
        std::string_view exchangeName = message.body();
            
        const std::string header = "device-id";
        auto& field = message.headers().get(header);

        if(!field.isInteger())
        {
            std::cerr << "Header (" + header + ") field invalid type" << std::endl;
            return;
        }
        
        int device_id = int(field);

        if(!visitor->visit_obsolete_src(device_id))
            return;

        std::cout << "Unregistered exchange: " << exchangeName << std::endl;

        auto que = this->get_binded_queue(exchangeName);

        if(que.has_value())
            channel.unbindQueue(exchangeName, que.value(), "");    
        
        channel.ack(deliveryTag);
        return;
    };

    return callback;
}

AMQP::MessageCallback rabbitmq_client::new_frame_msg_callback(detection_service_visitor<cv::Mat>* visitor)
{
    static AMQP::MessageCallback callback = [this, visitor](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered)
    {
        const std::string header = "device-id";

        auto& field = message.headers().get(header);
        if(!field.isInteger())
        {
            std::cerr << "Header (" + header + ") field invalid type" << std::endl;
            channel.ack(deliveryTag); // acknowledge anyway
            return;
        }

        auto source_id = int(field);

        cv::Mat rawData(1, message.bodySize(), CV_8SC1, (void*)message.body());
        auto decodedMat = new cv::Mat();
        cv::imdecode(rawData, cv::IMREAD_COLOR, decodedMat);

        auto& service = detection_service::get_service_instance();

        if(visitor->visit_new_frame(source_id, decodedMat))
            channel.ack(deliveryTag);
        
        return;
    };

    return callback;
}

