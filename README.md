UE Forums Thread
https://forums.unrealengine.com/development-discussion/vr-ar-development/89050-vr-openvr-expansion-plugin

Example Template Project
https://github.com/mordentral/VRExpPluginExample

Website:
www.vreue4.com

---

### Use Of This Plugin

This Plugin is intended to add additional functionality to all VR in UE5.1

### Plugin Website

[VREUE4.com](https://vreue4.com)

### How do I install it?

https://vreue4.com/documentation?section=installation

**Guides for migrating between different engine versions of the plugin:**

View the patch notes at www.vreue4.com for migration guides as well.

**Option 1:**

Go to www.vreue4.com and downloaded the pre-built binary version for the engine version you are using (not updated with every daily change, only weekly or with large patches).

Install it into your ProjectName/Plugins Directory (Engine level hasn't worked since 4.25 or so when it stopped letting me reference other plugins when compiling for that).

**Option 2 (More up to date - preferred if possible):**

-   Clone Or Download Zip and extract this repository into your "ProjectName/Plugins" directory, create this directory if it is missing.

-   Add the VRExpansionPlugin to your projects PublicDependencyModuleNames in the projects build.cs if you have c++ code included.

-   IF you do not have c++ code, use the Add New button in the editor and add a blank c++ class to your project.

-   Open up the generated project .SLN file and build the project from the build menu.

-   Copy over DefaultEngine, DefaultGameplayTags, DefaultScalability, and DefaultInput.ini files from the plugin folder to the Config folder of your project.

You need to have visual studio installed and follow the UE4 setup guide for it: https://docs.unrealengine.com/latest/INT/Programming/Development/VisualStudioSetup/

### How do I use it?

### How do I VR?

The template project contains use examples of most of the features of the plugin as well as locomotion modes, interaction methods, and basic multiplayer.

### How to build to Meta Quest 2

1. Install Android Studio Version: 4.0
2. After Installation and opening Android Studio:
    1. Configure->SDK Manager
    2. Go to "SDK Platforms" at the top
    3. Check "Show Package Details" at the bottom right
    4. Go to "SDK Tools" at the top
        1. Check "Android SDK Command-line Tools (latest)"
        2. Check "Android SDK Platform-Tools"
        3. Expand "NDK (Side-by-side)"
            1. Check version 25
    5. Click "Apply" and then "Finish"
3. Connect your headset and enable the prompts
4. Open Unreal Engine and open device manager
    1. Hover over Quest 2 and note the Operating System, Currently: Android 10, API level 29
    2. Make sure this version is installed in android studio: Configure->SDK Manager->SDK Tools
5. Settings in Unreal Engine:
    1. Platforms->Android
        - Configure Android license
        - Minimum SDK Version: 26
        - Target SDK Version: 26
    2. Platforms->Android SDK (Set these to your appropriate paths for android studio e.g. "C:/Users/user/AppData/Local/Android/Sdk")
        - Android NDK version must be >= 25 e.g. "C:/Users/user/AppData/Local/Android/Sdk/ndk/25.2.9519653"
        - Location of Java e.g. "C:/Program Files/Android/Android Studio/jre"
        - SDK API Level: matchndk
        - NDK API Level: android-21
