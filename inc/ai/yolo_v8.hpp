#pragma once

#ifndef YOLO_V8_H
#define YOLO_V8_H

#include "yolo.hpp"

class yolo_v8 : public yolo
{
    public:
        yolo_v8(const cv::Size2f& size, const std::string& dir, const std::string& model);
        virtual ~yolo_v8() = default;

        virtual const std::vector<std::string>& get_classes() override;
        virtual const std::vector<cv::Scalar>& get_colors() override;

        virtual const std::string_view get_model_name() override;
        virtual std::vector<detection> object_detection(const cv::Mat& img) override;
        virtual std::vector<std::vector<detection>> object_detection_batch(const std::vector<cv::Mat>& batch) override;
        virtual cv::Mat apply_detections_on_image(const cv::Mat& img, const std::vector<detection>& detections) override;
};

#endif // YOLO_V8_H