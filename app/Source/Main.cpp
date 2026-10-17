#include "ReferenceComponent.h"

#include <juce_gui_extra/juce_gui_extra.h>

namespace mixdebugger
{
class MixDebuggerApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "MixDebugger"; }
    const juce::String getApplicationVersion() override { return "0.2.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& commandLine) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName(), commandLine);
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        if (mainWindow != nullptr)
            mainWindow->openCommandLine(commandLine);
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name, const juce::String& commandLine)
            : DocumentWindow(name,
                             juce::Colour(0xff111827),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(commandLine), true);
            setResizable(true, true);
            setResizeLimits(1050, 800, 2200, 1600);
            centreWithSize(1180, 900);
            setVisible(true);
        }

        void openCommandLine(const juce::String& commandLine)
        {
            if (auto* content = dynamic_cast<MainComponent*>(getContentComponent()))
                content->openFromCommandLine(commandLine);
            toFront(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};
} // namespace mixdebugger

START_JUCE_APPLICATION(mixdebugger::MixDebuggerApplication)
