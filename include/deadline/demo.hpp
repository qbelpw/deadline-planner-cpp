#pragma once

#include "deadline/services.hpp"

namespace deadline {

class DemoScenarioBuilder final {
public:
    explicit DemoScenarioBuilder(DateTime now = DateTime::now()) noexcept;
    void populate(TaskService& service) const;

private:
    DateTime now_;
};

}  // namespace deadline
