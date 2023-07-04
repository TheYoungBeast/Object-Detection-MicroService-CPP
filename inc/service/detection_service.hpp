#pragma once

#ifndef DETECTION_SERVICE_H
#define DETECTION_SERVICE_H

#include <thread>

#include <opencv2/opencv.hpp>

#include "../ai/detection_model.hpp"
#include "background_service.hpp"

template <typename T>
class detection_service_visitor;

template <typename T>
class basic_detection_service;

template <typename T>
class prioritize_load_strategy;

template <typename T>
class prioritize_order_strategy;

typedef basic_detection_service<cv::Mat> detection_service;

struct performance_metrics
{
    const double avgProcessingTime{0};
    const double avgFPS{0};
    const unsigned sourcesCount{0};
    const unsigned long long totalFramesProcessed{0}; 
};

// SaS Singleton as Service
template <typename T>
class basic_detection_service : public background_service, public detection_service_visitor<T>
{
    using class_ref = basic_detection_service<T>&;
    using frame_queue = std::queue<std::shared_ptr<T>>;

    public:
        template <typename T2 = T>
        class processing_order_strategy {
            public:
                processing_order_strategy() = default;
                virtual ~processing_order_strategy() = default;

                virtual unsigned choose_next_queue(std::map<unsigned, frame_queue>& q, unsigned current_queue_id) = 0;
        };

        friend class processing_order_strategy<T>;

    private:
        const std::chrono::seconds sleep_on_empty{1};
        const unsigned max_size_per_que = 30;
        std::map<unsigned, frame_queue> queues{};
        std::map<unsigned, std::mutex> que_mutexes{};
        std::map<unsigned, long long> dropped_frames{};
        unsigned long long total_dropped_frames{0};
        unsigned long long total_frames_processed{0};
        cv::TickMeter performance_meter{};

        std::mutex inference_mutex{};
        std::unique_ptr<detection_model> model{};
        std::unique_ptr<processing_order_strategy<T>> strategy = std::unique_ptr<processing_order_strategy<T>>(new prioritize_order_strategy<T>());

    protected:
        basic_detection_service() = default;
        virtual ~basic_detection_service() = default;
        basic_detection_service(const basic_detection_service<T>&) = delete;
        basic_detection_service(basic_detection_service<T>&&) = delete;
        void operator=(const basic_detection_service<T>&) = delete;
        void operator=(basic_detection_service<T>&&) = delete;

    public:
        static class_ref get_service_instance();

        void use_model(std::unique_ptr<detection_model>& ptr);
        void use_model(std::unique_ptr<detection_model>&& ptr);

        bool register_source(const unsigned source_id);
        bool unregister_source(const unsigned source_id);

        performance_metrics get_performance();

        bool try_add_to_queue(const unsigned source_id, std::shared_ptr<T> frame);
        bool add_to_queue(const unsigned source_id, std::shared_ptr<T> frame);

        virtual std::thread run_background_service() override;

        bool contains(int source_id);

    private:
        virtual void run() override;

        // Visitor
    public:
        virtual bool visit_new_src(unsigned src_id) override;
        virtual bool visit_obsolete_src(unsigned src_id) override;
        virtual bool visit_new_frame(unsigned src_id, std::shared_ptr<T> frame) override;
};

template <typename T>
class prioritize_load_strategy : public basic_detection_service<T>::processing_order_strategy<T>
{
    public:
        virtual unsigned choose_next_queue(std::map<unsigned, std::queue<std::shared_ptr<T>>>& q, unsigned current_queue_id) override;
};

template <typename T>
class prioritize_order_strategy : public basic_detection_service<T>::processing_order_strategy<T>
{
    public:
        prioritize_order_strategy() = default;
        virtual ~prioritize_order_strategy() = default;
        virtual unsigned choose_next_queue(std::map<unsigned, std::queue<std::shared_ptr<T>>>& q, unsigned current_queue_id) override;
};

template <typename T = cv::Mat>
class detection_service_visitor
{
    public:
        detection_service_visitor() = default;
        virtual ~detection_service_visitor() = default;

        virtual bool visit_new_src(unsigned src_id) = 0;
        virtual bool visit_obsolete_src(unsigned src_id) = 0;
        virtual bool visit_new_frame(unsigned src_id, std::shared_ptr<T> frame) = 0;
};

#endif // DETECTION_SERVICE_H