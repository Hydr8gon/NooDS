/*
    Copyright 2019-2021 Hydr8gon

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

#include "../../core.h"
#include "../../settings.h"
#include "../../common/nds_icon.h"
#include "../../common/screen_layout.h"

int screenFilter = 1;
int showFpsCounter = 0;

std::string ndsPath, gbaPath;
Core *core = nullptr;
ScreenLayout layout;

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_FileBrowser_loadSettings(JNIEnv* env, jobject obj, jstring rootPath)
{
    // Convert the Java string to a C++ string
    const char *str = env->GetStringUTFChars(rootPath, nullptr);
    std::string path = str;
    env->ReleaseStringUTFChars(rootPath, str);

    // Define the platform settings
    std::vector<Setting> platformSettings =
    {
        Setting("screenFilter",   &screenFilter,   false),
        Setting("showFpsCounter", &showFpsCounter, false)
    };

    // Add the platform settings
    ScreenLayout::addSettings();
    Settings::add(platformSettings);

    // Load the settings
    // If this is the first time, set the path settings based on the root storage path
    if (!Settings::load(path + "/noods/noods.ini"))
    {
        Settings::setBios7Path(path + "/noods/bios7.bin");
        Settings::setBios9Path(path + "/noods/bios9.bin");
        Settings::setFirmwarePath(path + "/noods/firmware.bin");
        Settings::setGbaBiosPath(path + "/noods/gba_bios.bin");
        Settings::setSdImagePath(path + "/noods/sd.img");
        Settings::save();
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_FileBrowser_getNdsIcon(JNIEnv *env, jobject obj, jstring romName, jobject bitmap)
{
    // Get the NDS icon
    const char *str = env->GetStringUTFChars(romName, nullptr);
    NdsIcon icon(str);
    env->ReleaseStringUTFChars(romName, str);

    // Copy the data to the bitmap
    uint32_t *data;
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
        core = new Core(ndsPath, gbaPath);
        return 0;
    }
    catch (int e)
    {
        return e;
    }
}

extern "C" JNIEXPORT jstring JNICALL Java_com_hydra_noods_FileBrowser_getNdsPath(JNIEnv* env, jobject obj)
{
    return env->NewStringUTF(ndsPath.c_str());
}

extern "C" JNIEXPORT jstring JNICALL Java_com_hydra_noods_FileBrowser_getGbaPath(JNIEnv* env, jobject obj)
{
    return env->NewStringUTF(gbaPath.c_str());
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_FileBrowser_setNdsPath(JNIEnv* env, jobject obj, jstring value)
{
    const char *str = env->GetStringUTFChars(value, nullptr);
    ndsPath = str;
    env->ReleaseStringUTFChars(value, str);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_FileBrowser_setGbaPath(JNIEnv* env, jobject obj, jstring value)
{
    const char *str = env->GetStringUTFChars(value, nullptr);
    gbaPath = str;
    env->ReleaseStringUTFChars(value, str);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_fillAudioBuffer(JNIEnv *env, jobject obj, jshortArray buffer)
{
    // Get the buffer and sample data
    jshort *data = env->GetShortArrayElements(buffer, nullptr);
    int count = env->GetArrayLength(buffer) / 2;
    uint32_t *samples = core->spu.getSamples(count);

    // Fill the audio buffer
    for (int i = 0; i < count; i++)
    {
        data[i * 2 + 0] = samples[i] >>  0;
        data[i * 2 + 1] = samples[i] >> 16;
    }

    env->ReleaseShortArrayElements(buffer, data, 0);
    delete[] samples;
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_hydra_noods_NooRenderer_copyFramebuffer(JNIEnv *env, jobject obj, jobject bitmap, jboolean gbaCrop)
{
    // Get a new frame if one is ready
    uint32_t framebuffer[256 * 192 * 2];
    if (!core->gpu.getFrame(framebuffer, gbaCrop)) return false;

    // Copy the frame to the bitmap
    uint32_t *data;
    AndroidBitmap_lockPixels(env, bitmap, (void**)&data);
    memcpy(data, framebuffer, (gbaCrop ? (240 * 160) : (256 * 192 * 2)) * sizeof(uint32_t));
    AndroidBitmap_unlockPixels(env, bitmap);

    return true;
}

// The below functions are pretty much direct forwarders to core functions

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getDirectBoot(JNIEnv* env, jobject obj)
{
    return Settings::getDirectBoot();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getFpsLimiter(JNIEnv* env, jobject obj)
{
    return Settings::getFpsLimiter();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getThreaded2D(JNIEnv* env, jobject obj)
{
    return Settings::getThreaded2D();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getThreaded3D(JNIEnv* env, jobject obj)
{
    return Settings::getThreaded3D();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getScreenRotation(JNIEnv* env, jobject obj)
{
    return ScreenLayout::getScreenRotation();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getScreenArrangement(JNIEnv* env, jobject obj)
{
    return ScreenLayout::getScreenArrangement();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getScreenSizing(JNIEnv* env, jobject obj)
{
    return ScreenLayout::getScreenSizing();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getScreenGap(JNIEnv* env, jobject obj)
{
    return ScreenLayout::getScreenGap();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getIntegerScale(JNIEnv* env, jobject obj)
{
    return ScreenLayout::getIntegerScale();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getGbaCrop(JNIEnv* env, jobject obj)
{
    return ScreenLayout::getGbaCrop();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getScreenFilter(JNIEnv* env, jobject obj)
{
    return screenFilter;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_SettingsMenu_getShowFpsCounter(JNIEnv* env, jobject obj)
{
    return showFpsCounter;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setDirectBoot(JNIEnv* env, jobject obj, jint value)
{
    Settings::setDirectBoot(value);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setFpsLimiter(JNIEnv* env, jobject obj, jint value)
{
    Settings::setFpsLimiter(value);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setThreaded2D(JNIEnv* env, jobject obj, jint value)
{
    Settings::setThreaded2D(value);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setThreaded3D(JNIEnv* env, jobject obj, jint value)
{
    Settings::setThreaded3D(value);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setScreenRotation(JNIEnv* env, jobject obj, jint value)
{
    ScreenLayout::setScreenRotation(value);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setScreenArrangement(JNIEnv* env, jobject obj, jint value)
{
    ScreenLayout::setScreenArrangement(value);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setScreenSizing(JNIEnv* env, jobject obj, jint value)
{
    ScreenLayout::setScreenSizing(value);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setScreenGap(JNIEnv* env, jobject obj, jint value)
{
    ScreenLayout::setScreenGap(value);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setIntegerScale(JNIEnv* env, jobject obj, jint value)
{
    ScreenLayout::setIntegerScale(value);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setGbaCrop(JNIEnv* env, jobject obj, jint value)
{
    ScreenLayout::setGbaCrop(value);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setScreenFilter(JNIEnv* env, jobject obj, jint value)
{
    screenFilter = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_setShowFpsCounter(JNIEnv* env, jobject obj, jint value)
{
    showFpsCounter = value;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_SettingsMenu_saveSettings(JNIEnv* env, jobject obj)
{
    Settings::save();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooActivity_getShowFpsCounter(JNIEnv* env, jobject obj)
{
    return showFpsCounter;
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooActivity_getFps(JNIEnv *env, jobject obj)
{
    return core->getFps();
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_hydra_noods_NooActivity_isGbaMode(JNIEnv *env, jobject obj)
{
    return core->isGbaMode();
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
    core = new Core(ndsPath, gbaPath);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_pressScreen(JNIEnv *env, jobject obj, jint x, jint y)
{
    core->input.pressScreen();
    core->spi.setTouch(layout.getTouchX(x, y), layout.getTouchY(x, y));
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_releaseScreen(JNIEnv *env, jobject obj)
{
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

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getScreenRotation(JNIEnv* env, jobject obj)
{
    return ScreenLayout::getScreenRotation();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getGbaCrop(JNIEnv* env, jobject obj)
{
    return ScreenLayout::getGbaCrop();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getScreenFilter(JNIEnv* env, jobject obj)
{
    return screenFilter;
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooRenderer_updateLayout(JNIEnv *env, jobject obj, jint width, jint height)
{
    layout.update(width, height, core->isGbaMode());
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getTopX(JNIEnv *env, jobject obj)
{
    return layout.getTopX();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getBotX(JNIEnv *env, jobject obj)
{
    return layout.getBotX();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getTopY(JNIEnv *env, jobject obj)
{
    return layout.getTopY();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getBotY(JNIEnv *env, jobject obj)
{
    return layout.getBotY();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getTopWidth(JNIEnv *env, jobject obj)
{
    return layout.getTopWidth();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getBotWidth(JNIEnv *env, jobject obj)
{
    return layout.getBotWidth();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getTopHeight(JNIEnv *env, jobject obj)
{
    return layout.getTopHeight();
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_NooRenderer_getBotHeight(JNIEnv *env, jobject obj)
{
    return layout.getBotHeight();
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooButton_pressKey(JNIEnv *env, jobject obj, jint key)
{
    core->input.pressKey(key);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooButton_releaseKey(JNIEnv *env, jobject obj, jint key)
{
    core->input.releaseKey(key);
}
