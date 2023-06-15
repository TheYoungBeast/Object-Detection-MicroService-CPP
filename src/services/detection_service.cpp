#include "detection_service.hpp"

template <typename T>
basic_detection_service<T>& basic_detection_service<T>::get_service_instance()
{
    static basic_detection_service<T> service; // lazy init
    return service;
}

template <typename T>
std::thread basic_detection_service<T>::run_background_service()
{
    return std::thread([&](){ this->run_service(); });
}

template <typename T>
void basic_detection_service<T>::use_model(std::unique_ptr<detection_model>& ptr) {
    this->model = std::move(ptr);
}

template <typename T>
void basic_detection_service<T>::use_model(std::unique_ptr<detection_model>&& ptr) {
    this->use_model(ptr);
}

template <typename T>
bool basic_detection_service<T>::register_source(const unsigned source_id) {
    if(this->contains(source_id))
        return false;

    queues.try_emplace(source_id);
    que_mutexes.try_emplace(source_id);
    dropped_frames.try_emplace(source_id, 0);

    return true;
}

template <typename T>
bool basic_detection_service<T>::unregister_source(const unsigned source_id) {
    if(!this->contains(source_id))
        return false;

    std::lock_guard lock(inference_mutex); // block inference then safely remove
    queues.erase(source_id);
    que_mutexes.erase(source_id);
    dropped_frames.erase(source_id);

    return true;
}

template <typename T>
performance_metrics basic_detection_service<T>::get_performance() {
    return {
        performance_meter.getAvgTimeMilli(),
        performance_meter.getFPS(),
        static_cast<unsigned>(queues.size()),
        total_dropped_frames
    };
}

template <typename T>
bool basic_detection_service<T>::try_add_to_queue(const unsigned source_id, T* frame)
{
    if(!this->contains(source_id))
        return false;

    if(queues[source_id].size() >= max_size_per_que)
        return false;
    
    std::lock_guard lock(que_mutexes[source_id]);
    queues[source_id].emplace(frame);

    return true;
}

template <typename T>
bool basic_detection_service<T>::add_to_queue(const unsigned source_id, T* frame)
{
    throw std::runtime_error(std::string(__PRETTY_FUNCTION__) + std::string(" Not implemented yet"));
    return true;
}

template <typename T>
bool basic_detection_service<T>::contains(int source_id) {
    return queues.find(source_id) != queues.end();
}

template <typename T>
void basic_detection_service<T>::run_service()
{
    unsigned current_queue_id = 0;
    
    while (true)
    {
        if(queues.empty())
        {
            std::this_thread::sleep_for(sleep_on_empty);
            continue;
        }

        std::lock_guard inf_lock(inference_mutex);

        current_queue_id = strategy->choose_next_queue(this->queues, current_queue_id);

        if(current_queue_id > queues.size() || queues[current_queue_id].empty())
            continue;

        performance_meter.start();

        std::unique_lock lock(que_mutexes[current_queue_id]); // <-- locking queue

        std::unique_ptr<T> frame_ptr(queues[current_queue_id].front());
        queues[current_queue_id].pop(); // remove front position

        lock.unlock(); // unlock the queue

        try
        {
            auto&& results = model->object_detection(*frame_ptr);
            model->apply_detections_on_image(*frame_ptr, results);
            cv::imwrite("view.jpg", *frame_ptr);
            ++total_frames_processed;
        }
        catch(const cv::Exception& e)
        {
            std::cerr << e.what() << '\n';
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }

        performance_meter.stop();

        if(total_frames_processed == 1)
            performance_meter.reset();

        std::cout 
        << "Q:" << current_queue_id 
        << "\tP:" << queues[current_queue_id].size() 
        << "\tavgT:" << performance_meter.getAvgTimeMilli() << "ms"
        << "\tavgFPS:" << performance_meter.getFPS()
        << std::endl;
    }   
}

template <typename T>
unsigned prioritize_load_strategy<T>::choose_next_queue(std::map<unsigned, std::queue<T*>>& q, unsigned current_queue_id)
{
    auto it = std::max_element(q.begin(), q.end(), [](const std::pair<unsigned, std::queue<T*>> q1, const std::pair<unsigned, std::queue<T*>> q2) -> bool 
    {
        return q1.second.size() < q2.second.size();
    });

    auto dist = std::distance(q.begin(), it);
    return dist < 0 ? 0 : dist;
}

template <typename T>
unsigned priotitize_order_strategy<T>::choose_next_queue(std::map<unsigned, std::queue<T*>>& q, unsigned current_queue_id)
{
    return ++current_queue_id % q.size();
}

template class basic_detection_service<cv::Mat>;

//template class basic_detection_service<cv::GpuMat>;