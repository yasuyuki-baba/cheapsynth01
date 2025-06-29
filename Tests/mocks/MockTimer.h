#pragma once

#include <JuceHeader.h>
#include <functional>

namespace testing
{
    /**
     * Mock implementation of JUCE Timer class for testing
     * This allows manual simulation of timer callbacks without using actual timers
     */
    class MockTimer
    {
    public:
        MockTimer() = default;
        virtual ~MockTimer() = default;
        
        // Start the mock timer with the given interval
        void startTimer(int intervalMilliseconds)
        {
            isRunning = true;
            interval = intervalMilliseconds;
        }
        
        // Stop the mock timer
        void stopTimer()
        {
            isRunning = false;
        }
        
        // Check if the timer is running
        bool isTimerRunning() const
        {
            return isRunning;
        }
        
        // Get the timer interval
        int getTimerInterval() const
        {
            return interval;
        }
        
        // Manually trigger the timer callback
        void triggerTimerCallback()
        {
            if (isRunning && timerCallback)
            {
                timerCallback();
            }
        }
        
        // Set the timer callback function
        void setTimerCallback(std::function<void()> callback)
        {
            timerCallback = std::move(callback);
        }
        
    private:
        bool isRunning = false;
        int interval = 0;
        std::function<void()> timerCallback;
    };
}
