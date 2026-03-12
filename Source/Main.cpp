#include <JuceHeader.h>
#include "MainComponent.h"

class GlyphGridApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "GlyphGrid"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (juce::String name)
            : juce::DocumentWindow (std::move (name),
                                    juce::Colours::black,
                                    juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setResizable (true, true);
            setResizeLimits (1100, 760, 2200, 1600);
            setContentOwned (new MainComponent(), true);
            centreWithSize (1460, 920);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            const auto keyCode = std::tolower ((unsigned char) key.getKeyCode());
            const auto textChar = std::tolower ((unsigned char) key.getTextCharacter());

            if (keyCode == 'v' || textChar == 'v')
            {
                if (auto* content = dynamic_cast<MainComponent*> (getContentComponent()))
                    return content->toggleStageView();
            }

            return juce::DocumentWindow::keyPressed (key);
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (GlyphGridApplication)
