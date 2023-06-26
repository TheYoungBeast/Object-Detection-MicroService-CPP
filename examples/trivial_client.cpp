#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <thread>

// OpenCV
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/imgcodecs.hpp>

// Boost
#include <boost/asio/io_service.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/deadline_timer.hpp>

// AMQP RabbitMQ
#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>

using namespace std;
using namespace cv;
using namespace dnn;
using namespace boost;

int main()
{
    const std::string amqp = "amqp://localhost";
    const std::string mp4 = "../../assets/samples/dog-park-v2.mp4";
    const std::string AVB_SRC_EXCHANGE = "Available-Sources-Exchange";
    const std::string SRC_EXCHANGE = "source-99";
    const int src_id = 99;
    const std::string json = "{ \"id\": 99, \"exchange\": \"source-99\" }";
    auto total_published = 0;

    auto publisher = std::thread([&]()
    {
        boost::asio::io_service service(2);
        AMQP::LibBoostAsioHandler handler(service);
        AMQP::TcpConnection connection (&handler, AMQP::Address(amqp));
        AMQP::TcpChannel channel (&connection);

        auto error_callback = [](const char* msg) {
            std::cerr << msg << std::endl;
        };

        channel.onError(error_callback);

        auto t = std::thread([&](){ service.run(); });

        channel.declareExchange(AVB_SRC_EXCHANGE, AMQP::ExchangeType::fanout).onError(error_callback);
        channel.declareExchange(SRC_EXCHANGE, AMQP::ExchangeType::fanout).onError(error_callback);

        channel.publish(AVB_SRC_EXCHANGE, "", json);

        cv::VideoCapture cap(mp4);

        if(!cap.isOpened())
        {
            std::cerr << "Cannot open video file" << std::endl;
            return -1;
        }

        static bool once = false;

        while(cap.grab())
        {
            cv::Mat frame;
            cap.read(frame);

            if(frame.empty())
            {
                std::cout << "EOF" << std::endl;
                break;
            }
            
            int size = frame.total() * frame.elemSize();
            try
            {
                AMQP::Envelope envelope(reinterpret_cast<const char*>(frame.data), size);

                AMQP::Table table;
                table.set("srcid", src_id);
                table.set("imgtype", frame.type());
                table.set("imgwidth", frame.cols);
                table.set("imgheight", frame.rows);
                envelope.setHeaders(table);

                total_published++;
                channel.publish(SRC_EXCHANGE, "", envelope);
                this_thread::sleep_for(std::chrono::milliseconds(30));
            }
            catch(const std::exception& e) {
                std::cout << e.what() << std::endl;
            }

            frame.release();
        }

        std::cout << total_published << " frames published" << std::endl;
        cap.release();
        service.stop();
        t.join();
        return 0;
    });

    std::thread consumer = std::thread([&]()
    {
        boost::asio::io_service service(2);
        AMQP::LibBoostAsioHandler handler(service);
        AMQP::TcpConnection connection (&handler, AMQP::Address(amqp));
        AMQP::TcpChannel channel (&connection);
        static int counter = 0;

        channel.declareQueue(AMQP::exclusive).onSuccess([&](const std::string &name, uint32_t messagecount, uint32_t consumercount)
        {
            channel.bindQueue("detection-results-99", name, "");

            auto& consumer = channel.consume(name);
            consumer.onMessage([&](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered)
            {
                channel.ack(deliveryTag);
                std::string str(message.body(), message.bodySize());
                std::cout << ++counter << " frame results: \t" << str << std::endl;

                if(counter == total_published) {
                    service.stop();
                }
            });
        });
        

        service.run();
    });

    publisher.join();
    consumer.join();

    cv::destroyAllWindows();
    return 0;
}