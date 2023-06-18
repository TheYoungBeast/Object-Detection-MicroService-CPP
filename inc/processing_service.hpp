#pragma once

#ifndef PROCESSING_SERVICE_H
#define PROCESSING_SERVICE_H

#include <map>
#include <set>
#include <vector>
#include <memory>

#include <opencv2/opencv.hpp>

#include "background_service.hpp"
#include "detection_model.hpp"
#include "data_publisher.hpp"

template <typename T>
class basic_processing_service;

typedef basic_processing_service<cv::Mat> processing_service;

// SaS Singleton as Service
template <typename T = cv::Mat>
class basic_processing_service : public background_service
{   
    using self_ptr = basic_processing_service<T>*;

    protected:
        float g_confidence_threshold = {0.6f};
        unsigned boxes_limit = 0;
        std::map<unsigned, float> thresholds;
        std::set<unsigned> excluded;
        std::vector<std::string> labels;
        std::queue< std::tuple<unsigned, std::shared_ptr<T>, std::vector<detection>> > results;
        std::mutex sync;
        std::shared_ptr<data_publisher> json_publisher;

    protected:
        basic_processing_service() = default;

        T& apply_results(T& img, const std::vector<detection>& results);

        virtual void run() override;

    public:
        self_ptr set_labels(std::vector<std::string>& labels);
        self_ptr exclude_objects(std::vector<unsigned> excluded);
        self_ptr set_objects_threshold(std::map<unsigned, float>& map);
        self_ptr set_objects_threshold(const std::pair<unsigned, float>& pair);
        self_ptr set_applied_detections_limit(unsigned limit);
        self_ptr set_global_threshold(float th);
        self_ptr set_publishers(std::shared_ptr<data_publisher> publisher_data, std::shared_ptr<data_publisher> publisher_frame);
        
        void push_results(unsigned src_id, std::shared_ptr<T> frame, const std::vector<detection>& detections);

        static self_ptr get_service_instance();

        virtual std::thread run_background_service() override;

        virtual ~basic_processing_service() = default;
};




#endif // PROCESSING_SERVICE_H