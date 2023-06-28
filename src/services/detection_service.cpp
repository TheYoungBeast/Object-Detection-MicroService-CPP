#include "../inc/service/detection_service.hpp"
#include "../inc/service/processing_service.hpp"

template <typename T>
basic_detection_service<T>& basic_detection_service<T>::get_service_instance()
{
    static basic_detection_service<T> service; // lazy init
    return service;
}

template <typename T>
std::thread basic_detection_service<T>::run_background_service()
{
    return std::thread([&](){ this->run(); });
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
bool basic_detection_service<T>::try_add_to_queue(const unsigned source_id, std::shared_ptr<T> frame)
{
    if(!this->contains(source_id))
        return false;

    if(queues[source_id].size() >= max_size_per_que)
        return false;
    
    std::lock_guard lock(que_mutexes[source_id]);
    queues[source_id].push(frame);

    return true;
}

template <typename T>
bool basic_detection_service<T>::add_to_queue(const unsigned source_id, std::shared_ptr<T> frame)
{
    throw std::runtime_error(std::string(__PRETTY_FUNCTION__) + std::string(" Not implemented yet"));
    return true;
}

template <typename T>
bool basic_detection_service<T>::contains(int source_id) {
    return queues.find(source_id) != queues.end();
}

template <typename T>
void basic_detection_service<T>::run()
{
    unsigned current_queue_id = 0;
    
    while (true)
    {
        try{
            if(queues.empty())
            {
                //std::this_thread::sleep_for(sleep_on_empty);
                std::this_thread::yield();
                continue;
            }

            std::lock_guard inf_lock(inference_mutex);

            current_queue_id = strategy->choose_next_queue(this->queues, current_queue_id);

            if(queues[current_queue_id].empty())
                continue;

            performance_meter.start();

            auto frame_ptr = queues[current_queue_id].front(); // fetch

            if(!frame_ptr) 
                continue;
            
            auto results = model->object_detection(*frame_ptr);
            
            ++total_frames_processed;

            auto processing = processing_service::get_service_instance();
            processing->push_results(current_queue_id, frame_ptr, results);
        
            performance_meter.stop();

            if(total_frames_processed == 1)
                performance_meter.reset();

            if(total_frames_processed % 60 == 0)
            {
                spdlog::debug(
                    "que: {} \tque size: {} \tavg: {:.2f}ms \t{:.2f}fps \t{} frames", 
                    current_queue_id, 
                    queues[current_queue_id].size(), 
                    performance_meter.getAvgTimeMilli(), 
                    performance_meter.getFPS(),
                    total_frames_processed);
            }

            std::unique_lock lock(que_mutexes[current_queue_id]); // <-- locking queue
            queues[current_queue_id].pop(); // remove front position
            lock.unlock(); // unlock the queue
        }
        catch(const cv::Exception& e) {
            spdlog::error(e.what());
        }
        catch(const std::exception& e) {
           spdlog::error(e.what());
        }
        catch(...) {
            spdlog::error("[Detection service]: Unknown Error");
        }
    }   
}


template <typename T>
bool basic_detection_service<T>::visit_new_src(unsigned src_id) 
{   
    auto metrics = this->get_performance();
    spdlog::info("[Service Metrics]: {}ms \t{} fps", metrics.avgProcessingTime, metrics.avgFPS);

    return this->register_source(src_id);
}

template <typename T>
bool basic_detection_service<T>::visit_obsolete_src(unsigned src_id){
    return this->unregister_source(src_id);
}

template <typename T>
bool basic_detection_service<T>::visit_new_frame(unsigned src_id, std::shared_ptr<T> frame) {
    return this->try_add_to_queue(src_id, frame);
}

template <typename T>
unsigned prioritize_load_strategy<T>::choose_next_queue(std::map<unsigned, std::queue<std::shared_ptr<T>>>& q, unsigned current_queue_id)
{
    auto it = std::max_element(q.begin(), q.end(), 
    [](const std::pair<unsigned, std::queue<std::shared_ptr<T>>> q1, const std::pair<unsigned,std::queue<std::shared_ptr<T>>> q2) -> bool 
    {
        return q1.second.size() < q2.second.size();
    });

    return (*it).first;
}

template <typename T>
unsigned priotitize_order_strategy<T>::choose_next_queue(std::map<unsigned, std::queue<std::shared_ptr<T>>>& q, unsigned current_queue_id)
{
    auto it = q.find(current_queue_id);

    if(it == q.end())
        return (*q.begin()).first;

    if(it++ != q.end())
        return (*it).first;

    return (*q.begin()).first;
}

template class basic_detection_service<cv::Mat>;
template class detection_service_visitor<cv::Mat>;
//template class basic_detection_service<cv::GpuMat>;