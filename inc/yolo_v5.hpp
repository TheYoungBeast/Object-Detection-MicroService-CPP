#pragma once
#ifndef YOLO_V5_H
#define YOLO_V5_H

#include <string>
#include <vector>

#include "yolo_v8.hpp"

class yolo_v5 : public yolo_v8
{
    public:
        yolo_v5(cv::Size2f shape, const std::string& dir, const std::string& model);
        virtual ~yolo_v5() = default;

        virtual std::vector<detection> object_detection(const cv::Mat& img) override;
};

#endif // YOLO_V5_H