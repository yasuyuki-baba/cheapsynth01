#pragma once
#include <memory>
#include <utility>

namespace cs01 {
// UI-thread lifecycle guard. Async dialogs must not mutate a closed/replaced
// editor or dereference its processor after destruction. No audio-thread use.
class EditorSession {
   public:
    void begin() {
        lifetime = std::make_shared<const int>(0);
    }
    void end() {
        lifetime.reset();
    }
    template <typename Action>
    auto wrap(Action action) const {
        return [weak = std::weak_ptr<const int>(lifetime),
                action = std::move(action)](auto&&... args) {
            if (!weak.expired())
                action(std::forward<decltype(args)>(args)...);
        };
    }

   private:
    std::shared_ptr<const int> lifetime;
};
}  // namespace cs01
