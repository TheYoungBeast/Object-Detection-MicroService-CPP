#pragma once

#ifndef BACKGROUND_SERVICE_HPP
#define BACKGROUND_SERVICE_HPP

#include <thread>

class background_service
{
    public:
        background_service() = default;
        virtual ~background_service() = default;

        /**
         * Runs service in the background
         * @returns std::thread that the service is running on
        */
        virtual std::thread run_background_service() = 0;

    protected:
        /**
         * Runs service' task
        */
        virtual void run() = 0;
};


#endif