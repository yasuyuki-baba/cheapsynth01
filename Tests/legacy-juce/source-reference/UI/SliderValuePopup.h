#pragma once

#include <JuceHeader.h>

// One persistent popup, with a single owner for both text and position updates.
class SliderValuePopup : public juce::BubbleComponent, private juce::Slider::Listener {
   public:
    explicit SliderValuePopup(juce::Component& parent) : host(parent) {
        host.addChildComponent(this);
        setInterceptsMouseClicks(false, false);
        setAlwaysOnTop(true);
    }

    ~SliderValuePopup() override {
        for (auto* slider : sliders) {
            slider->removeMouseListener(this);
            slider->removeListener(this);
        }
    }

    void attach(juce::Slider& slider) {
        sliders.add(&slider);
        slider.setPopupDisplayEnabled(false, false, nullptr);
        slider.addMouseListener(this, false);
        slider.addListener(this);
    }

    void refresh() {
        if (active != nullptr && isVisible())
            update();
    }

    void showForSlider(juce::Slider& slider) {
        if (sliders.contains(&slider))
            show(&slider);
    }

   private:
    void mouseEnter(const juce::MouseEvent& event) override {
        if (!dragging)
            show(dynamic_cast<juce::Slider*>(event.eventComponent));
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (!dragging) {
            active = nullptr;
            setVisible(false);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override {
        dragging = true;
        show(dynamic_cast<juce::Slider*>(event.eventComponent));
    }

    void mouseUp(const juce::MouseEvent&) override {
        dragging = false;
        if (active != nullptr && !active->isMouseOver()) {
            active = nullptr;
            setVisible(false);
        }
    }

    void sliderValueChanged(juce::Slider* slider) override {
        if (slider == active)
            update();
    }

    void show(juce::Slider* slider) {
        if (slider == nullptr)
            return;
        if (slider != active) {
            active = slider;
            const auto bounds = host.getLocalArea(slider, slider->getLocalBounds());
            setAllowedPlacement(bounds.getCentreX() < host.getWidth() / 2
                                    ? juce::BubbleComponent::right
                                    : juce::BubbleComponent::left);
        }
        update();
        setVisible(true);
        toFront(false);
    }

    void update() {
        const auto newText = active->getTextFromValue(active->getValue());
        if (text != newText) {
            text = newText;
            repaint();
        }
        auto target = host.getLocalArea(active, active->getLocalBounds());
        if (active->getSliderStyle() == juce::Slider::LinearVertical) {
            const auto thumb = host.getLocalPoint(
                active,
                juce::Point<int>(active->getWidth() / 2,
                                 juce::roundToInt(active->getPositionOfValue(active->getValue()))));
            target = {thumb.x - 8, thumb.y, 16, 1};
        }
        setPosition(target);
    }

    void getContentSize(int& width, int& height) override {
        const auto font = getLookAndFeel().getSliderPopupFont(*active);
        // Fixed minimum width avoids breathing as the number of digits changes.
        width = juce::jmax(100, juce::GlyphArrangement::getStringWidthInt(font, text) + 18);
        height = juce::roundToInt(font.getHeight() * 1.6f);
    }

    void paintContent(juce::Graphics& g, int width, int height) override {
        if (active == nullptr)
            return;
        g.setFont(getLookAndFeel().getSliderPopupFont(*active));
        g.setColour(active->findColour(juce::TooltipWindow::textColourId, true));
        g.drawFittedText(text, {0, 0, width, height}, juce::Justification::centred, 1);
    }

    juce::Component& host;
    juce::Array<juce::Slider*> sliders;
    juce::Slider* active = nullptr;
    juce::String text;
    bool dragging = false;
};