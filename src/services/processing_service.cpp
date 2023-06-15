#include "processing_service.hpp"

template<typename T>
processing_service<T>* processing_service<T>::exclude_objects(std::vector<unsigned> excluded) {
    for(const auto class_id: excluded)
        this->excluded.insert(class_id);

    return this;
}

template<typename T>
processing_service<T>* processing_service<T>::set_objects_threshold(std::map<unsigned, float>& map)
{
    this->thresholds.merge(map);

    return this;
}

template<typename T>
processing_service<T>* processing_service<T>::set_objects_threshold(const std::pair<unsigned, float>& pair)
{
    this->thresholds.try_emplace(pair.first, pair.second);

    return this;
}

template<typename T>
processing_service<T>* processing_service<T>::set_labels(std::vector<std::string>& labels) {
    this->labels = labels;

    return this;
}

template<typename T>
processing_service<T>* processing_service<T>::set_applied_detections_limit(unsigned limit) {
    this->boxes_limit = limit;

    return this;
}

template<typename T>
processing_service<T>* processing_service<T>::set_global_threshold(float th) {
    this->g_confidence_threshold = th;

    return this;
}
        
template<typename T>
processing_service<T>* processing_service<T>::get_service_instance() {
    static processing_service<T> service; // lazdy

    return &service;
}

template <typename T>
std::thread processing_service<T>::run_background_service()
{
    return std::thread([&]() { this->run(); });
}

template <typename T>
T& processing_service<T>::apply_results(T& img, const std::vector<detection>& results)
{
    if(results.empty())
        return img;
    
    unsigned int counter = 0;

    for (int i = 0; i < results.size(); ++i)
    {
        auto& detection = results[i];

        const auto is_excluded = this->excluded.find(detection.class_id) != this->excluded.end();
        if(is_excluded)
            continue; // skip, this class obj is excluded from processing
        
        if(boxes_limit != 0 && counter == this->boxes_limit)
            break;

        const auto has_own_threshold = this->thresholds.find(detection.class_id) != this->thresholds.end();
        if(has_own_threshold)
        {
            if(this->thresholds[detection.class_id] > detection.confidence)
                continue;
        }
        else
        {
            if(this->g_confidence_threshold > detection.confidence)
                continue;
        }

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

template <typename T>
void processing_service<T>::run()
{
    while(true)
    {
        std::this_thread::sleep_for(std::chrono::hours(1));
    }
}

template class processing_service<cv::Mat>;