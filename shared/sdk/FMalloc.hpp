#pragma once

namespace sdk {
class FMalloc {
public:
    static FMalloc* get();

    virtual ~FMalloc() {}

private:
};
}