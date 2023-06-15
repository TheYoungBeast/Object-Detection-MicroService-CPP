#include <random>

#include "../inc/yolo_v8.hpp"


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

void yolo_v8::load_model() 
{
    this->network = cv::dnn::readNetFromONNX(this->dir_path+this->model_name);
    
    std::cout << "\nRunning on CUDA" << std::endl;
    this->network.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
    this->network.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
    //this->network.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
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
    cv::Mat modelInput = img;
    if (letterBoxForSquare && model_shape.width == model_shape.height)
        modelInput = formatToSquare(modelInput);

    bool swapRB = true;

    cv::Mat blob;
    cv::dnn::blobFromImage(modelInput, blob, 1.0f/255.0f, model_shape, cv::Scalar(), swapRB, false);
    this->network.setInput(blob);

    std::vector<cv::Mat> outputs;
    this->network.forward(outputs, this->network.getUnconnectedOutLayersNames());

    int rows = outputs[0].size[1];
    int dimensions = outputs[0].size[2];

    bool yolov8 = false;
    // yolov5 has an output of shape (batchSize, 25200, 85) (Num classes + box[x,y,w,h] + confidence[c])
    // yolov8 has an output of shape (batchSize, 84,  8400) (Num classes + box[x,y,w,h])
    if (dimensions > rows) // Check if the shape[2] is more than shape[1] (yolov8)
    {
        yolov8 = true;
        rows = outputs[0].size[2];
        dimensions = outputs[0].size[1];

        outputs[0] = outputs[0].reshape(1, dimensions);
        cv::transpose(outputs[0], outputs[0]);
    }
    float *data = (float *)outputs[0].data;

    float x_factor = (modelInput.cols*1.0f) / model_shape.width;
    float y_factor = (modelInput.rows*1.0f) / model_shape.height;

    std::vector<int> class_ids;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    for (int i = 0; i < rows; ++i)
    {
        if (yolov8)
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
        }
        else // yolov5
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

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(100, 255);
        result.color = cv::Scalar(dis(gen),
                                  dis(gen),
                                  dis(gen));

        result.class_name = classes[result.class_id];
        result.box = boxes[idx];

        std::cout << result.class_name << std::endl;
        detections.push_back(result);
    }

    return detections;
}

cv::Mat yolo_v8::apply_detections_on_image(const cv::Mat& img, const std::vector<detection>& detections) 
{
    static float const confidance_threshold = 0.55f;

    if(detections.empty())
        return img;
    
    for (int i = 0; i < detections.size(); ++i)
    {
        auto detection = detections[i];

        // if (detection.confidence < confidance_threshold)
        //     continue;

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