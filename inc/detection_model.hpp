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
        /**
         * Loads dnn network model from the path passed in the constructor
        */
        virtual void load_model() = 0;

        /**
         * Loads classes (objects labels)
        */
        virtual void load_classes() = 0;

    protected:
        detection_model(const cv::Size2f& size ,const std::string& dir, const std::string& model) 
            : model_name(model), dir_path(dir), model_shape(size) {};

        detection_model(const detection_model&) = delete;
        detection_model(detection_model&&) = delete;
        void operator= (const detection_model&) = delete;

    public:
        /**
         * @returns name of dnn model (e.g. yolov8.onnx)
        */
        virtual const std::string_view get_model_name() = 0;

        /**
         * @returns classes that a model is able to detect
        */
        virtual auto get_classes() -> const std::vector<std::string>& = 0;

        /**
         * @returns colors associated with object classes
        */
        virtual auto get_colors() -> const std::vector<cv::Scalar>& = 0;

        /**
         * @brief Performs object detection on a given image
         * @param img The image on which object detection will be performed
         * @returns list of all detected objects
        */
        virtual auto object_detection(const cv::Mat& img) -> std::vector<detection> = 0;
        
        /**
         * Performs object detection on a batch of images
         * @param batch batch of images
         * @returns list for each image in the batch with detected objects per image
        */
        virtual auto object_detection_batch(const std::vector<cv::Mat>& batch) -> std::vector<std::vector<detection>> = 0;
        
        /**
         * @breif Applies detection results (bounding boxes) directly on a given image.
         * @param img source image
         * @param detections list of bounding boxes to apply
         * @note It will apply the whole detection vector you passed to the function.
         * @note Filter results before applying them to the image
         * @returns modified image
        */
        virtual cv::Mat apply_detections_on_image(const cv::Mat& img, const std::vector<detection>& detections) = 0;

    public:
        virtual ~detection_model() = default;
};

#endif // DETECTION_MODEL_H