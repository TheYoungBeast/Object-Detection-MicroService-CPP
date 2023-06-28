#pragma once

#ifndef IMG_PUBLISHER_HPP
#define IMG_PUBLISHER_HPP

#include <opencv2/opencv.hpp>

#include "../rabbitmq/rabbitmq_client.hpp"
#include "basic_publisher.hpp"

template <typename T> 
class basic_img_publisher;

typedef basic_img_publisher<cv::Mat>  img_publisher;

template <typename T = cv::Mat> 
class basic_img_publisher : public basic_publisher
{
    private:
        std::string prefix = "processed-img-";

    public:
        basic_img_publisher(std::shared_ptr<rabbitmq_client>& client);

        /**
         * @param img Image
         * @param srcid source id
        */
        virtual void publish_image(T& img, unsigned srcid);

        virtual ~basic_img_publisher() = default;
};

#endif // IMG_PUBLISHER_HPP