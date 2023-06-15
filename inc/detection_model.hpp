#pragma once

#ifndef DETECTION_MODEL_H
#define DETECTION_MODEL_H

#include <string>
#include <vector>

// OpenCV2
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>

struct detection
{
    int class_id = 0;
    std::string class_name{};
    float confidence = 0.0f;
    cv::Scalar color{};
    cv::Rect box{};
};

class detection_model
{
    protected:
        cv::dnn::Net network;
        const std::string model_name;
        const std::string dir_path;
        const cv::Size2f model_shape;
        std::vector<std::string> classes{};

    protected:
        virtual void load_model() = 0;
        virtual void load_classes() = 0;

    protected:
        detection_model(const cv::Size2f& size ,const std::string& dir, const std::string& model) 
            : model_name(model), dir_path(dir), model_shape(size) {};

        detection_model(const detection_model&) = delete;
        detection_model(detection_model&&) = delete;
        void operator= (const detection_model&) = delete;

    public:
        virtual const std::string_view get_model_name() = 0;

        virtual auto get_classes() -> const std::vector<std::string>& = 0;
        virtual auto get_colors() -> const std::vector<cv::Scalar>& = 0;

        virtual auto object_detection(const cv::Mat& img) -> std::vector<detection> = 0;
        virtual auto object_detection_batch(const std::vector<cv::Mat>& batch) -> std::vector<std::vector<detection>> = 0;
        
        virtual cv::Mat apply_detections_on_image(const cv::Mat& img, const std::vector<detection>& detections) = 0;

    public:
        virtual ~detection_model() = default;
};

#endif // DETECTION_MODEL_H