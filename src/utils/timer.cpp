#include <chrono>

namespace battle {

class Timer {
public:
    void start() {
        start_ = std::chrono::high_resolution_clock::now();
    }

    double elapsed_ms() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }

    double elapsed_sec() const {
        return elapsed_ms() / 1000.0;
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

}
