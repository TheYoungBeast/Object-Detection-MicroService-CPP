#pragma once

#ifndef YOLO_V8_H
#define YOLO_V8_H

#include "detection_model.hpp"

class yolo_v8 : public detection_model
{
    protected:
        float modelConfidenceThreshold {0.25f};
        float modelScoreThreshold      {0.45f};
        float modelNMSThreshold        {0.50f};

        bool letterBoxForSquare = true;

        std::vector<cv::Scalar> colors{};

    private:
        virtual void load_model() override;
        virtual void load_classes() override;

    protected:
        virtual cv::Mat formatToSquare(const cv::Mat& source);

    public:
        yolo_v8(const cv::Size2f& size, const std::string& dir, const std::string& model);
        virtual ~yolo_v8() = default;

        virtual const std::string_view get_model_name() override;
        virtual std::vector<detection> object_detection(const cv::Mat& img) override;
        virtual cv::Mat apply_detections_on_image(const cv::Mat& img, const std::vector<detection>& detections) override;
};

#endif // YOLO_V8_H