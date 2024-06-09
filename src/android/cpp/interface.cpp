/*
    Copyright 2019-2024 Hydr8gon

    This file is part of NooDS.

    NooDS is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    NooDS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with NooDS. If not, see <https://www.gnu.org/licenses/>.
*/

#include <android/bitmap.h>
#include <jni.h>
#include <string>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "../../core.h"
#include "../../settings.h"
#include "../../common/nds_icon.h"
#include "../../common/screen_layout.h"

int micEnable = 0;
int showFpsCounter = 0;
int buttonScale = 5;
int buttonSpacing = 10;
int vibrateStrength = 1;
int keyBinds[15] = {};

std::string ndsPath = "", gbaPath = "";
int ndsRomFd = -1, gbaRomFd = -1;
int ndsSaveFd = -1, gbaSaveFd = -1;
int ndsStateFd = -1, gbaStateFd = -1;
Core *core = nullptr;
ScreenLayout layout;
uint32_t framebuffer[256 * 192 * 8];

SLEngineItf audioEngine;
SLObjectItf audioEngineObj;
SLObjectItf audioMixerObj;
SLObjectItf audioPlayerObj;
SLObjectItf audioRecorderObj;
SLPlayItf audioPlayer;
SLRecordItf audioRecorder;
SLAndroidSimpleBufferQueueItf audioPlayerQueue;
SLAndroidSimpleBufferQueueItf audioRecorderQueue;
int16_t audioPlayerBuffer[1024 * 2];
int16_t audioRecorderBuffer[1024];

void audioPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    // Get 699 samples at 32768Hz, which is equal to approximately 1024 samples at 48000Hz
    uint32_t *original = core->spu.getSamples(699);

    // Stretch the 699 samples out to 1024 samples in the audio buffer
    for (int i = 0; i < 1024; i++)
    {
        uint32_t sample = original[i * 699 / 1024];
        audioPlayerBuffer[i * 2 + 0] = sample >>  0;
        audioPlayerBuffer[i * 2 + 1] = sample >> 16;
    }

    (*audioPlayerQueue)->Enqueue(audioPlayerQueue, audioPlayerBuffer, sizeof(audioPlayerBuffer));
    delete[] original;
}

void audioRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    // Send microphone samples to the core
    core->spi.sendMicData(audioRecorderBuffer, 1024, 48000);
    (*audioRecorderQueue)->Enqueue(audioRecorderQueue, audioRecorderBuffer, sizeof(audioRecorderBuffer));
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_hydra_noods_FileBrowser_loadSettings(JNIEnv* env, jobject obj, jstring rootPath)
{
    // Convert the Java string to a C++ string
    const char *str = env->GetStringUTFChars(rootPath, nullptr);
    std::string path = str;
    env->ReleaseStringUTFChars(rootPath, str);

    // Define the platform settings
    std::vector<Setting> platformSettings =
    {
        Setting("micEnable", &micEnable, false),
        Setting("showFpsCounter", &showFpsCounter, false),
        Setting("buttonScale", &buttonScale, false),
        Setting("buttonSpacing", &buttonSpacing, false),
        Setting("vibrateStrength", &vibrateStrength, false),
        Setting("keyA", &keyBinds[0], false),
        Setting("keyB", &keyBinds[1], false),
        Setting("keySelect", &keyBinds[2], false),
        Setting("keyStart", &keyBinds[3], false),
        Setting("keyRight", &keyBinds[4], false),
        Setting("keyLeft", &keyBinds[5], false),
        Setting("keyUp", &keyBinds[6], false),
        Setting("keyDown", &keyBinds[7], false),
        Setting("keyR", &keyBinds[8], false),
        Setting("keyL", &keyBinds[9], false),
        Setting("keyX", &keyBinds[10], false),
        Setting("keyY", &keyBinds[11], false),
        Setting("keyFastHold", &keyBinds[12], false),
        Setting("keyFastToggle", &keyBinds[13], false),
        Setting("keyScreenSwap", &keyBinds[14], false)
    };

    // Add the platform settings
    ScreenLayout::addSettings();
    Settings::add(platformSettings);

    // Load the settings
    if (Settings::load(path + "/noods.ini"))
        return true;

    // If this is the first time, set the path settings based on the root storage path
    Settings::bios7Path = path + "/bios7.bin";
    Settings::bios9Path = path + "/bios9.bin";
    Settings::firmwarePath = path + "/firmware.bin";
    Settings::gbaBiosPath = path + "/gba_bios.bin";
    Settings::sdImagePath = path + "/sd.img";
    Settings::save();
    return false;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_FileBrowser_getNdsIcon(JNIEnv *env, jobject obj, jint fd, jobject bitmap)
{
    // Read an NDS icon and copy its data to the bitmap
    uint32_t *data;
    NdsIcon icon("", fd);
    AndroidBitmap_lockPixels(env, bitmap, (void**)&data);
    memcpy(data, icon.getIcon(), 32 * 32 * sizeof(uint32_t));
    AndroidBitmap_unlockPixels(env, bitmap);
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_FileBrowser_startCore(JNIEnv* env, jobject obj)
{
    // Start the core, or return an error code on failure
    try
    {
        if (core) delete core;
        core = new Core(ndsPath, gbaPath, 0, ndsRomFd, gbaRomFd, ndsSaveFd, gbaSaveFd, ndsStateFd, gbaStateFd);
        return 0;
    }
    catch (CoreError e)
    {
        return e;
    }
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_hydra_noods_FileBrowser_isNdsLoaded(JNIEnv* env, jobject obj)
{
    // Check if an NDS ROM has been loaded
    return ndsPath != "" || ndsRomFd != -1;
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_hydra_noods_FileBrowser_isGbaLoaded(JNIEnv* env, jobject obj)
{
    // Check if a GBA ROM has been loaded
    return gbaPath != "" || gbaRomFd != -1;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_FileBrowser_setNdsPath(JNIEnv* env, jobject obj, jstring value)
{
    // Set the NDS ROM file path
    const char *str = env->GetStringUTFChars(value, nullptr);
    ndsPath = str;
    env->ReleaseStringUTFChars(value, str);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_FileBrowser_setGbaPath(JNIEnv* env, jobject obj, jstring value)
{
    // Set the GBA ROM file path
    const char *str = env->GetStringUTFChars(value, nullptr);
    gbaPath = str;
    env->ReleaseStringUTFChars(value, str);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_FileBrowser_setNdsFds(JNIEnv* env, jobject obj, jint romFd, jint saveFd, jint stateFd)
{
    // Set the NDS ROM file descriptors
    ndsRomFd = romFd;
    ndsSaveFd = saveFd;
    ndsStateFd = stateFd;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_FileBrowser_setGbaFds(JNIEnv* env, jobject obj, jint romFd, jint saveFd, jint stateFd)
{
    // Set the GBA ROM file descriptors
    gbaRomFd = romFd;
    gbaSaveFd = saveFd;
    gbaStateFd = stateFd;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_startAudioPlayer(JNIEnv* env, jobject obj)
{
    // Initialize the audio engine
    slCreateEngine(&audioEngineObj, 0, nullptr, 0, nullptr, nullptr);
    (*audioEngineObj)->Realize(audioEngineObj, SL_BOOLEAN_FALSE);
    (*audioEngineObj)->GetInterface(audioEngineObj, SL_IID_ENGINE, &audioEngine);
    (*audioEngine)->CreateOutputMix(audioEngine, &audioMixerObj, 0, 0, 0);
    (*audioMixerObj)->Realize(audioMixerObj, SL_BOOLEAN_FALSE);

    // Define the audio output format
    SLDataFormat_PCM audioFormat =
    {
        SL_DATAFORMAT_PCM,
        2,
        SL_SAMPLINGRATE_48,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
        SL_BYTEORDER_LITTLEENDIAN
    };

    // Initialize the audio player
    SLDataLocator_AndroidSimpleBufferQueue bufferLoc = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataSource audioSource = {&bufferLoc, &audioFormat};
    SLDataLocator_OutputMix audioMixer = {SL_DATALOCATOR_OUTPUTMIX, audioMixerObj};
    SLDataSink audioSink = {&audioMixer, nullptr};
    SLInterfaceID ids[2] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
    SLboolean reqs[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    (*audioEngine)->CreateAudioPlayer(audioEngine, &audioPlayerObj, &audioSource, &audioSink, 2, ids, reqs);

    // Set up the audio player queue and callback
    (*audioPlayerObj)->Realize(audioPlayerObj, SL_BOOLEAN_FALSE);
    (*audioPlayerObj)->GetInterface(audioPlayerObj, SL_IID_PLAY, &audioPlayer);
    (*audioPlayerObj)->GetInterface(audioPlayerObj, SL_IID_BUFFERQUEUE, &audioPlayerQueue);
    (*audioPlayerQueue)->RegisterCallback(audioPlayerQueue, audioPlayerCallback, nullptr);
    (*audioPlayer)->SetPlayState(audioPlayer, SL_PLAYSTATE_PLAYING);

    // Initiate playback with an empty buffer
    memset(audioPlayerBuffer, 0, sizeof(audioPlayerBuffer));
    (*audioPlayerQueue)->Enqueue(audioPlayerQueue, audioPlayerBuffer, sizeof(audioPlayerBuffer));
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_startAudioRecorder(JNIEnv* env, jobject obj)
{
    // Define the audio input source
    SLDataLocator_IODevice deviceLoc =
    {
        SL_DATALOCATOR_IODEVICE,
        SL_IODEVICE_AUDIOINPUT,
        SL_DEFAULTDEVICEID_AUDIOINPUT,
        nullptr
    };

    // Define the audio input format
    SLDataFormat_PCM audioFormat =
    {
        SL_DATAFORMAT_PCM,
        1,
        SL_SAMPLINGRATE_48,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_SPEAKER_FRONT_CENTER,
        SL_BYTEORDER_LITTLEENDIAN
    };

    // Initialize the audio recorder
    SLDataSource audioSource = {&deviceLoc, nullptr};
    SLDataLocator_AndroidSimpleBufferQueue bufferLoc = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataSink audioSink = {&bufferLoc, &audioFormat};
    SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    SLboolean req[1] = {SL_BOOLEAN_TRUE};
    (*audioEngine)->CreateAudioRecorder(audioEngine, &audioRecorderObj, &audioSource, &audioSink, 1, id, req);

    // Set up the audio recorder queue and callback
    (*audioRecorderObj)->Realize(audioRecorderObj, SL_BOOLEAN_FALSE);
    (*audioRecorderObj)->GetInterface(audioRecorderObj, SL_IID_RECORD, &audioRecorder);
    (*audioRecorderObj)->GetInterface(audioRecorderObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &audioRecorderQueue);
    (*audioRecorderQueue)->RegisterCallback(audioRecorderQueue, audioRecorderCallback, nullptr);
    (*audioRecorder)->SetRecordState(audioRecorder, SL_RECORDSTATE_RECORDING);

    // Initiate recording with an empty buffer
    memset(audioRecorderBuffer, 0, sizeof(audioRecorderBuffer));
    (*audioRecorderQueue)->Enqueue(audioRecorderQueue, audioRecorderBuffer, sizeof(audioRecorderBuffer));
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_stopAudioPlayer(JNIEnv* env, jobject obj)
{
    // Clean up the audio player objects
    (*audioPlayerObj)->Destroy(audioPlayerObj);
    (*audioMixerObj)->Destroy(audioMixerObj);
    (*audioEngineObj)->Destroy(audioEngineObj);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_stopAudioRecorder(JNIEnv* env, jobject obj)
{
    // Clean up the audio recorder object
    (*audioRecorderObj)->Destroy(audioRecorderObj);
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_hydra_noods_NooRenderer_copyFramebuffer(JNIEnv *env, jobject obj, jobject bitmap, jboolean gbaCrop)
{
    // Get a new frame if one is ready
    if (!core->gpu.getFrame(framebuffer, gbaCrop))
        return false;

    // Copy the frame to the bitmap
    uint32_t *data;
    AndroidBitmap_lockPixels(env, bitmap, (void**)&data);
    int shift = (Settings::highRes3D || Settings::screenFilter == 1) * 2;
    size_t count = (gbaCrop ? (240 * 160) : (256 * 192 * 2)) << shift;
    memcpy(data, framebuffer, count * sizeof(uint32_t));
    AndroidBitmap_unlockPixels(env, bitmap);
    return true;
}

// The below functions are pretty much direct forwarders to core functions

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_BindingsPreference_getKeyBind(JNIEnv* env, jobject obj, jint index)
{
    return keyBinds[index];
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_BindingsPreference_setKeyBind(JNIEnv* env, jobject obj, jint index, jint value)
{
    keyBinds[index] = value;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getDirectBoot(JNIEnv* env, jobject obj)
{
    return Settings::directBoot;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getFpsLimiter(JNIEnv* env, jobject obj)
{
    return Settings::fpsLimiter;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getThreaded2D(JNIEnv* env, jobject obj)
{
    return Settings::threaded2D;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getThreaded3D(JNIEnv* env, jobject obj)
{
    return Settings::threaded3D;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getHighRes3D(JNIEnv* env, jobject obj)
{
    return Settings::highRes3D;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getScreenPosition(JNIEnv* env, jobject obj)
{
    return ScreenLayout::screenPosition;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getScreenRotation(JNIEnv* env, jobject obj)
{
    return ScreenLayout::screenRotation;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getScreenArrangement(JNIEnv* env, jobject obj)
{
    return ScreenLayout::screenArrangement;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getScreenSizing(JNIEnv* env, jobject obj)
{
    return ScreenLayout::screenSizing;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getScreenGap(JNIEnv* env, jobject obj)
{
    return ScreenLayout::screenGap;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getScreenFilter(JNIEnv* env, jobject obj)
{
    return Settings::screenFilter;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getIntegerScale(JNIEnv* env, jobject obj)
{
    return ScreenLayout::integerScale;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getGbaCrop(JNIEnv* env, jobject obj)
{
    return ScreenLayout::gbaCrop;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getScreenGhost(JNIEnv* env, jobject obj)
{
    return Settings::screenGhost;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getMicEnable(JNIEnv* env, jobject obj)
{
    return micEnable;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getShowFpsCounter(JNIEnv* env, jobject obj)
{
    return showFpsCounter;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getButtonScale(JNIEnv* env, jobject obj)
{
    return buttonScale;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getButtonSpacing(JNIEnv* env, jobject obj)
{
    return buttonSpacing;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getVibrateStrength(JNIEnv* env, jobject obj)
{
    return vibrateStrength;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setDirectBoot(JNIEnv* env, jobject obj, jint value)
{
    Settings::directBoot = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setFpsLimiter(JNIEnv* env, jobject obj, jint value)
{
    Settings::fpsLimiter = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setThreaded2D(JNIEnv* env, jobject obj, jint value)
{
    Settings::threaded2D = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setThreaded3D(JNIEnv* env, jobject obj, jint value)
{
    Settings::threaded3D = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setHighRes3D(JNIEnv* env, jobject obj, jint value)
{
    Settings::highRes3D = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setScreenPosition(JNIEnv* env, jobject obj, jint value)
{
    ScreenLayout::screenPosition = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setScreenRotation(JNIEnv* env, jobject obj, jint value)
{
    ScreenLayout::screenRotation = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setScreenArrangement(JNIEnv* env, jobject obj, jint value)
{
    ScreenLayout::screenArrangement = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setScreenSizing(JNIEnv* env, jobject obj, jint value)
{
    ScreenLayout::screenSizing = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setScreenGap(JNIEnv* env, jobject obj, jint value)
{
    ScreenLayout::screenGap = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setScreenFilter(JNIEnv* env, jobject obj, jint value)
{
    Settings::screenFilter = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setIntegerScale(JNIEnv* env, jobject obj, jint value)
{
    ScreenLayout::integerScale = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setGbaCrop(JNIEnv* env, jobject obj, jint value)
{
    ScreenLayout::gbaCrop = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setScreenGhost(JNIEnv* env, jobject obj, jint value)
{
    Settings::screenGhost = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setMicEnable(JNIEnv* env, jobject obj, jint value)
{
    micEnable = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setShowFpsCounter(JNIEnv* env, jobject obj, jint value)
{
    showFpsCounter = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setButtonScale(JNIEnv* env, jobject obj, jint value)
{
    buttonScale = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setButtonSpacing(JNIEnv* env, jobject obj, jint value)
{
    buttonSpacing = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setVibrateStrength(JNIEnv* env, jobject obj, jint value)
{
    vibrateStrength = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_saveSettings(JNIEnv* env, jobject obj)
{
    Settings::save();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooActivity_getFps(JNIEnv *env, jobject obj)
{
    return core->fps;
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_hydra_noods_NooActivity_isGbaMode(JNIEnv *env, jobject obj)
{
    return core->gbaMode;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_runFrame(JNIEnv *env, jobject obj)
{
    core->runFrame();
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_writeSave(JNIEnv *env, jobject obj)
{
    core->cartridgeNds.writeSave();
    core->cartridgeGba.writeSave();
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_restartCore(JNIEnv *env, jobject obj)
{
    if (core) delete core;
    core = new Core(ndsPath, gbaPath, 0, ndsRomFd, gbaRomFd, ndsSaveFd, gbaSaveFd, ndsStateFd, gbaStateFd);
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooActivity_checkState(JNIEnv *env, jobject obj)
{
    return core->saveStates.checkState();
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_hydra_noods_NooActivity_saveState(JNIEnv *env, jobject obj)
{
    return core->saveStates.saveState();
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_hydra_noods_NooActivity_loadState(JNIEnv *env, jobject obj)
{
    return core->saveStates.loadState();
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_pressScreen(JNIEnv *env, jobject obj, jint x, jint y)
{
    if (core->gbaMode) return;
    core->input.pressScreen();
    core->spi.setTouch(layout.getTouchX(x, y), layout.getTouchY(x, y));
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_releaseScreen(JNIEnv *env, jobject obj)
{
    if (core->gbaMode) return;
    core->input.releaseScreen();
    core->spi.clearTouch();
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_resizeGbaSave(JNIEnv *env, jobject obj, jint size)
{
    core->cartridgeGba.resizeSave(size);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_resizeNdsSave(JNIEnv *env, jobject obj, jint size)
{
    core->cartridgeNds.resizeSave(size);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooRenderer_updateLayout(JNIEnv *env, jobject obj, jint width, jint height)
{
    layout.update(width, height, core->gbaMode);
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getTopX(JNIEnv *env, jobject obj)
{
    return layout.topX;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getBotX(JNIEnv *env, jobject obj)
{
    return layout.botX;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getTopY(JNIEnv *env, jobject obj)
{
    return layout.topY;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getBotY(JNIEnv *env, jobject obj)
{
    return layout.botY;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getTopWidth(JNIEnv *env, jobject obj)
{
    return layout.topWidth;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getBotWidth(JNIEnv *env, jobject obj)
{
    return layout.botWidth;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getTopHeight(JNIEnv *env, jobject obj)
{
    return layout.topHeight;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getBotHeight(JNIEnv *env, jobject obj)
{
    return layout.botHeight;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooButton_pressKey(JNIEnv *env, jobject obj, jint key)
{
    core->input.pressKey(key);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooButton_releaseKey(JNIEnv *env, jobject obj, jint key)
{
    core->input.releaseKey(key);
}
