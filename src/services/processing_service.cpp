#include "../inc/service/processing_service.hpp"

template<typename T>
basic_processing_service<T>* basic_processing_service<T>::exclude_objects(std::vector<unsigned> excluded) {
    for(const auto class_id: excluded)
        this->excluded.insert(class_id);

    return this;
}

template<typename T>
basic_processing_service<T>* basic_processing_service<T>::set_objects_threshold(std::map<unsigned, float>& map)
{
    this->thresholds.merge(map);

    return this;
}

template<typename T>
basic_processing_service<T>* basic_processing_service<T>::set_objects_threshold(const std::pair<unsigned, float>& pair)
{
    this->thresholds.try_emplace(pair.first, pair.second);

    return this;
}

template<typename T>
basic_processing_service<T>* basic_processing_service<T>::set_labels(std::vector<std::string>& labels) {
    this->labels = labels;

    return this;
}

template<typename T>
basic_processing_service<T>* basic_processing_service<T>::set_applied_detections_limit(unsigned limit) {
    this->boxes_limit = limit;

    return this;
}

template<typename T>
basic_processing_service<T>* basic_processing_service<T>::set_global_threshold(float th) {
    this->g_confidence_threshold = th;

    return this;
}
        
template<typename T>
basic_processing_service<T>* basic_processing_service<T>::get_service_instance() {
    static basic_processing_service<T> service; // lazdy

    return &service;
}

template <typename T>
std::thread basic_processing_service<T>::run_background_service()
{
    return std::thread([&]() { this->run(); });
}

template <typename T>
T& basic_processing_service<T>::apply_results(T& img, const std::vector<detection>& results)
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
void basic_processing_service<T>::push_results(unsigned src_id, std::shared_ptr<T> frame, const std::vector<detection>& detections)
{
    std::lock_guard lock(sync);
    results.emplace(std::make_tuple(src_id, frame, detections));
}

template<typename T>
basic_processing_service<T>* basic_processing_service<T>::set_data_publisher(std::shared_ptr<data_publisher> publisher)
{
    this->json_publisher = publisher;
    return this;
}

template<typename T>
basic_processing_service<T>* basic_processing_service<T>::set_img_publisher(std::shared_ptr<img_publisher> publisher)
{
    this->frame_publisher = publisher;
    return this;
}

template <typename T>
void basic_processing_service<T>::run()
{
    while(true)
    {
        if(results.empty())
        {
            //std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        auto& [id, frame, detections] = results.front();

        if(json_publisher)
            json_publisher->publish(id, detections, 5);

        if(frame_publisher)
        {
            auto& result = this->apply_results(*frame, detections);
            frame_publisher->publish_image(result, id);
        }

        std::unique_lock lock(sync);
        results.pop();
        lock.unlock();
    }
}

template class basic_processing_service<cv::Mat>;