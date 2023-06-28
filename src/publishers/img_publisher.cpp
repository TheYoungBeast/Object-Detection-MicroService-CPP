#include "../inc/publisher/img_publisher.hpp"

template <typename T> 
basic_img_publisher<T>::basic_img_publisher(std::shared_ptr<rabbitmq_client>& _client)
    : basic_publisher( _client )
{
}


template <typename T> 
void basic_img_publisher<T>::publish_image(T& img, unsigned srcid)
{
    int size = img.total() * img.elemSize();
    try
    {
        AMQP::Envelope envelope(reinterpret_cast<const char*>(img.data), size);

        AMQP::Table table;
        table.set("srcid", srcid);
        table.set("imgtype", img.type());
        table.set("imgwidth", img.cols);
        table.set("imgheight", img.rows);
        envelope.setHeaders(table);

        if(!is_declared(srcid))
            declare_exchange(srcid, this->prefix);
    
        rabbitmq->publish(declared_exchanges[srcid], "", envelope);
    }
    catch(const std::exception& e) {
        spdlog::error(e.what());
    }
}


template class basic_img_publisher<cv::Mat>;