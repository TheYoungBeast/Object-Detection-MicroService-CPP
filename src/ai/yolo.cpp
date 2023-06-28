#include "../inc/ai/yolo.hpp"

yolo::yolo(const cv::Size2f& size, const std::string& dir, const std::string& model)
    : detection_model(size, dir, model)
{
    this->load_classes();
    this->load_model();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(50, 255);
    
    for(int i = 0; i < classes.size(); i++)
        colors.push_back(cv::Scalar(dis(gen), dis(gen), dis(gen)));

    assert(colors.size() == classes.size());
}


void yolo::load_classes() {
    static std::vector<std::string> classes{"person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch", "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"};

    this->classes = classes;
}

void yolo::load_model() 
{
    this->network = cv::dnn::readNetFromONNX(this->dir_path+this->model_name);
    
    spdlog::info("Running on CUDA");
    spdlog::info("Loaded model {}", this->dir_path+this->model_name);

    this->network.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
    this->network.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
}

cv::Mat yolo::formatToSquare(const cv::Mat& source)
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