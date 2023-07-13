#pragma once

#ifndef ACTION_STORAGE_HPP
#define ACTION_STORAGE_HPP

#include <vector>
#include <functional>

/**
 * @brief Stores functions/lambdas as actions. 
 * @brief Bind context to any function and wrap it in lambda expression to redo the given action later.
*/
class action_storage
{
    private:
        std::vector<std::function<void()>> actions;

    public:
        action_storage() = default;
        ~action_storage() = default;

        void push(std::function<void()>& action) { actions.emplace_back(action); }
        void clear() { actions.clear(); }
        auto size() -> std::size_t { return actions.size(); }
        auto pop() -> std::function<void()> {
            auto action = actions.at(0);
            actions.erase(actions.begin());
            return action;
        }
        auto get_all() -> std::vector<std::function<void()>> { return actions; }
};

#endif // ACTION_STORAGE_HPP