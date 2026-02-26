#pragma once

#include <memory>
#include "IMSSystem.h"

class IMSServerState {
public:
    IMSServerState() {
        current_ = iMS::IMSSystem::Create();
    }

    std::shared_ptr<iMS::IMSSystem> get() const {
        return current_;
    }

    void set(std::shared_ptr<iMS::IMSSystem> ims) {
        current_ = std::move(ims);
    }

private:
    std::shared_ptr<iMS::IMSSystem> current_;
};
