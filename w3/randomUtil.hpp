#include <numeric>
#include <algorithm>
#include <iterator>
#include <random>
#include <ranges>
#include <numeric>


namespace rnd{
    namespace views = std::ranges::views;

    inline auto& get_engine() {
        static std::random_device device;
        static std::default_random_engine engine{device()};
        return engine;
    }

    inline auto sample(auto& rengine, auto& container, const auto& weights, size_t num = 1) {
        using iter_type = decltype(container.begin());
        std::vector<iter_type> result;
        result.reserve(num);
        float total_weight = std::accumulate(weights.begin(), weights.end(), 0.0f);
        auto normalized = weights | views::transform([&](float weight) {return weight / total_weight;});
        std::vector<float> borders;
        borders.reserve(weights.size());
        std::partial_sum(normalized.begin(), normalized.end(), std::back_inserter(borders));
        // with repeats
        std::uniform_real_distribution<float> distr(0.0f, 1.0f);
        for (size_t generated = 0; generated < num; ++generated) {
            float random = distr(rengine);
            auto item = std::lower_bound(borders.begin(), borders.end(), random);
            if (item == borders.end()) {
                --generated;
                continue;
            }
            result.push_back(container.begin() + (item - borders.begin())); 
        }
        return result;
    }
};
