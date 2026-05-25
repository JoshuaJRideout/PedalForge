#include <juce_core/system/juce_TargetPlatform.h>

#if JucePlugin_Build_Standalone

#include <juce_audio_plugin_client/detail/juce_IncludeSystemHeaders.h>
#include <juce_audio_plugin_client/detail/juce_IncludeModuleHeaders.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace juce
{

//==============================================================================
class CustomStandaloneFilterWindow : public StandaloneFilterWindow
{
public:
    CustomStandaloneFilterWindow (const String& title,
                                  Colour backgroundColour,
                                  std::unique_ptr<StandalonePluginHolder> pluginHolderIn)
        : StandaloneFilterWindow (title, backgroundColour, std::move (pluginHolderIn))
    {
        // 1. Force the system's native OS title bar
        setUsingNativeTitleBar (true);
        
        // 2. Explicitly enable ALL traffic light buttons (Close, Minimize, AND Maximize/Fullscreen)
        // (JUCE's default standalone wrapper deliberately disables the maximize button)
        setTitleBarButtonsRequired (DocumentWindow::allButtons, false);
        
        // 3. Make sure the window tells the OS it can be resized/fullscreened
        setResizable (true, true);
    }
};

//==============================================================================
class CustomStandaloneFilterApp : public JUCEApplication
{
public:
    CustomStandaloneFilterApp()
    {
        PropertiesFile::Options options;
        options.applicationName     = CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #else
        options.folderName          = "";
       #endif
        appProperties.setStorageParameters (options);
    }

    const String getApplicationName() override              { return CharPointer_UTF8 (JucePlugin_Name); }
    const String getApplicationVersion() override           { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override              { return true; }
    void anotherInstanceStarted (const String&) override    {}

    StandaloneFilterWindow* createWindow()
    {
        if (Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            jassertfalse;
            return nullptr;
        }

        return new CustomStandaloneFilterWindow (getApplicationName(),
                                                 LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
                                                 createPluginHolder());
    }

    std::unique_ptr<StandalonePluginHolder> createPluginHolder()
    {
        constexpr auto autoOpenMidiDevices =
       #if (JUCE_ANDROID || JUCE_IOS) && ! JUCE_DONT_AUTO_OPEN_MIDI_DEVICES_ON_MOBILE
                true;
       #else
                false;
       #endif

       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr StandalonePluginHolder::PluginInOuts channels[] { JucePlugin_PreferredChannelConfigurations };
        const Array<StandalonePluginHolder::PluginInOuts> channelConfig (channels, juce::numElementsInArray (channels));
       #else
        const Array<StandalonePluginHolder::PluginInOuts> channelConfig;
       #endif

        return std::make_unique<StandalonePluginHolder> (appProperties.getUserSettings(),
                                                         false,
                                                         String{},
                                                         nullptr,
                                                         channelConfig,
                                                         autoOpenMidiDevices);
    }

    void initialise (const String& commandLine) override
    {
        // Set up file logger in ~/Library/Logs/PedalForge/PedalForge.log
        auto logDir = File::getSpecialLocation (File::userHomeDirectory)
                        .getChildFile ("Library/Logs/PedalForge");
        logDir.createDirectory();
        auto logFile = logDir.getChildFile ("PedalForge.log");

        // Delete old log file if it's too big
        if (logFile.existsAsFile() && logFile.getSize() > 10 * 1024 * 1024)
            logFile.deleteFile();

        fileLogger = std::make_unique<FileLogger> (logFile, "PedalForge Startup Log", 1024 * 1024);
        Logger::setCurrentLogger (fileLogger.get());

        Logger::writeToLog ("==================================================");
        Logger::writeToLog ("PedalForge Standalone Startup");
        Logger::writeToLog ("Version: " + getApplicationVersion());
        Logger::writeToLog ("Command Line: " + commandLine);
        Logger::writeToLog ("Log File: " + logFile.getFullPathName());
        Logger::writeToLog ("==================================================");

        mainWindow.reset (createWindow());

        if (mainWindow != nullptr)
        {
           #if JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE
            Desktop::getInstance().setKioskModeComponent (mainWindow.get(), false);
           #endif
            mainWindow->setVisible (true);
        }
        else
        {
            pluginHolder = createPluginHolder();
        }
    }

    void shutdown() override
    {
        pluginHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();

        Logger::writeToLog ("PedalForge Standalone Shutdown");
        Logger::setCurrentLogger (nullptr);
        fileLogger.reset();
    }

    void systemRequestedQuit() override
    {
        if (pluginHolder != nullptr)
            pluginHolder->savePluginState();

        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            Timer::callAfterDelay (100, []()
            {
                if (auto app = JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

protected:
    ApplicationProperties appProperties;
    std::unique_ptr<StandaloneFilterWindow> mainWindow;

private:
    std::unique_ptr<StandalonePluginHolder> pluginHolder;
    std::unique_ptr<juce::FileLogger> fileLogger;
};

} // namespace juce

//==============================================================================
// This is the custom application hook that JUCE calls instead of the default
// standalone app creation macro when JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1.
juce::JUCEApplicationBase* juce_CreateApplication()
{
    return new juce::CustomStandaloneFilterApp();
}

void OpenStandaloneAudioSettingsDialog()
{
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        holder->showAudioSettingsDialog();
}

#endif // JucePlugin_Build_Standalone
