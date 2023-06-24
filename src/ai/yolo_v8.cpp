#include <random>
#include <spdlog/spdlog.h>

#include "yolo_v8.hpp"


yolo_v8::yolo_v8(const cv::Size2f& size, const std::string& dir, const std::string& model)
    : detection_model(size, dir, model)
{
    load_model();
    load_classes();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(50, 255);
    
    for(int i = 0; i < classes.size(); i++)
        colors.push_back(cv::Scalar(dis(gen), dis(gen), dis(gen)));

    assert(colors.size() == classes.size());
}

void yolo_v8::load_classes() {
    static std::vector<std::string> classes{"person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch", "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"};

    this->classes = classes;
}

const std::string_view yolo_v8::get_model_name() {
    return this->model_name;
}

const std::vector<std::string>& yolo_v8::get_classes(){
    return this->classes;
}

const std::vector<cv::Scalar>&  yolo_v8::get_colors(){
    return this->colors;
}

void yolo_v8::load_model() 
{
    this->network = cv::dnn::readNetFromONNX(this->dir_path+this->model_name);
    
    spdlog::info("Running on CUDA");

    this->network.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
    this->network.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
}

cv::Mat yolo_v8::formatToSquare(const cv::Mat& source)
{
    int col = source.cols;
    int row = source.rows;
    int _max = MAX(col, row);

    if (col == _max && row == _max)
        return source;

    cv::Mat result = cv::Mat::zeros(_max, _max, CV_8UC3);
    source.copyTo(result(cv::Rect(0, 0, col, row)));
    return result;
}

std::vector<detection> yolo_v8::object_detection(const cv::Mat& img) {
    return this->object_detection_batch({img}).at(0);
}


std::vector<std::vector<detection>> yolo_v8::object_detection_batch(const std::vector<cv::Mat>& batch) 
{
    /* DYNAMIC BATCH SIZE IS NOT SUPPORTED AFAIR */
    /* BUT MAYBE I WILL FIND WORKAROUND LATER */
    /* FOR NOW PROCCES THIS FIRST IMG FROM BATCH */

    if(batch.empty())
        return {};

    cv::Mat modelInput = batch[0];

    if (letterBoxForSquare && model_shape.width == model_shape.height)
        modelInput = formatToSquare(modelInput);

    if(modelInput.empty())
        return {};

    const bool swapRB = true;
    const unsigned batchSize = 1;

    cv::Mat blob;
    cv::dnn::blobFromImage(modelInput, blob, 1.0f/255.0f, model_shape, cv::Scalar(), swapRB, false);
    this->network.setInput(blob);

    std::vector<cv::Mat> outputs;
    this->network.forward(outputs, this->network.getUnconnectedOutLayersNames());

    std::vector<std::vector<detection>> batch_detections{};

    // yolov8 has an output of shape (batchSize, 84,  8400) (Num classes + box[x,y,w,h])
    for(unsigned batch = 0; batch < batchSize; batch++)
    {
        int rows = outputs[batch].size[1];
        int dimensions = outputs[batch].size[2];
        
        rows = outputs[batch].size[2];
        dimensions = outputs[batch].size[1];

        outputs[batch] = outputs[batch].reshape(1, dimensions);
        cv::transpose(outputs[batch], outputs[batch]);
        
        float *data = (float *)outputs[batch].data;

        float x_factor = (modelInput.cols*1.0f) / model_shape.width;
        float y_factor = (modelInput.rows*1.0f) / model_shape.height;

        std::vector<int> class_ids;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;

        for (int i = 0; i < rows; ++i)
        {
            float *classes_scores = data+4;

            cv::Mat scores(1, classes.size(), CV_32FC1, classes_scores);
            cv::Point class_id;
            double maxClassScore;

            minMaxLoc(scores, 0, &maxClassScore, 0, &class_id);

            if (maxClassScore > this->modelConfidenceThreshold)
            {
                confidences.push_back(maxClassScore);
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

        batch_detections.emplace_back(detections);
    }

    return batch_detections;
}

cv::Mat yolo_v8::apply_detections_on_image(const cv::Mat& img, const std::vector<detection>& detections) 
{
    if(detections.empty())
        return img;
    
    for (int i = 0; i < detections.size(); ++i)
    {
        auto detection = detections[i];

        auto box = detection.box;
        auto classId = detection.class_id;
        const auto& color = detection.color;
        cv::rectangle(img, box, color, 3);
        
        std::ostringstream stringStream;
        stringStream << detection.class_name << " - " << std::setprecision(4) << detection.confidence*100 << "%";

        cv::rectangle(img, cv::Point(box.x, box.y - 20), cv::Point(box.x + box.width, box.y), color, cv::FILLED);
        cv::putText(img,  stringStream.str(), cv::Point(box.x, box.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));
    }

    return img;
}