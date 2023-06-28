#pragma once

#ifndef YOLO_HPP
#define YOLO_HPP

#include <random>
#include <spdlog/spdlog.h>

#include "detection_model.hpp"

class yolo : public detection_model
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
        yolo(const cv::Size2f& size, const std::string& dir, const std::string& model);
        virtual ~yolo() = default;
};


#endif // YOLO_HPP