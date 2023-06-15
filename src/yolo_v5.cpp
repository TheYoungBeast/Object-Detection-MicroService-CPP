#include "../inc/yolo_v5.hpp"

yolo_v5::yolo_v5(cv::Size2f shape, const std::string& dir, const std::string& model)
    : yolo_v8(shape, dir, model)
{
}

std::vector<detection> yolo_v5::object_detection(const cv::Mat& frame)
{
    cv::Mat modelInput = frame;
    if (letterBoxForSquare && model_shape.width == model_shape.height)
        modelInput = formatToSquare(modelInput);

    cv::Mat blob;
    cv::dnn::blobFromImage(modelInput, blob, 1.0/255.0, model_shape, cv::Scalar(), true, false);
    this->network.setInput(blob);

    std::vector<cv::Mat> outputs;
    this->network.forward(outputs, this->network.getUnconnectedOutLayersNames());

    int rows = outputs[0].size[1];
    int dimensions = outputs[0].size[2];

    float *data = (float *)outputs[0].data;

    float x_factor = modelInput.cols / model_shape.width;
    float y_factor = modelInput.rows / model_shape.height;

    std::vector<int> class_ids;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    for (int i = 0; i < rows; ++i)
    {
        float confidence = data[4];

        if (confidence >= this->modelConfidenceThreshold)
        {
            float *classes_scores = data+5;

            cv::Mat scores(1, classes.size(), CV_32FC1, classes_scores);
            cv::Point class_id;
            double max_class_score;

            minMaxLoc(scores, 0, &max_class_score, 0, &class_id);

            if (max_class_score > modelScoreThreshold)
            {
                confidences.push_back(confidence);
                class_ids.push_back(class_id.x);

                float x = data[0];
                float y = data[1];
                float w = data[2];
                float h = data[3];

                int left = int((x - 0.5 * w) * x_factor);
                int top = int((y - 0.5 * h) * y_factor);

                int width = int(w * x_factor);
                int height = int(h * y_factor);

                boxes.push_back(cv::Rect(left, top, width, height));
            }
        }

        data += dimensions;
    }

    std::vector<int> nms_result;
    cv::dnn::NMSBoxes(boxes, confidences, modelScoreThreshold, modelNMSThreshold, nms_result);

    std::vector<detection> detections{};
    for (unsigned long i = 0; i < nms_result.size(); ++i)
    {
        int idx = nms_result[i];

        detection result;
        result.class_id = class_ids[idx];
        result.confidence = confidences[idx];
        result.color = colors[result.class_id];
        result.class_name = classes[result.class_id];
        result.box = boxes[idx];

        detections.push_back(result);
    }

    return detections;
}