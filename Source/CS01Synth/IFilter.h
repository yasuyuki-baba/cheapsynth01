#pragma once

/**
 * Common interface for filters
 * Provides information about filter-specific behaviors
 */
class IFilter {
   public:
    // Resonance mode enumeration
    enum class ResonanceMode {
        Toggle,     // Toggle control (Original)
        Continuous  // Continuous value control (Modern)
    };

    // Virtual destructor
    virtual ~IFilter() = default;

    // Pure virtual function to get resonance mode
    virtual ResonanceMode getResonanceMode() const = 0;

    // Other filter-related methods that can be extended in the future
    // virtual FilterType getFilterType() const = 0;
    // virtual int getFilterOrder() const = 0;
    // etc...
};
