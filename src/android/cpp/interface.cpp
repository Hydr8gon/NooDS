#include <android/bitmap.h>
#include <jni.h>
#include <string>

#include "../../core.h"
#include "../../settings.h"

Core *core = nullptr;

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_FileBrowser_loadSettings(JNIEnv* env, jobject obj, jstring rootPath)
{
    // Convert the Java string to a C++ string
    const char *str = env->GetStringUTFChars(rootPath, nullptr);
    std::string path = str;
    env->ReleaseStringUTFChars(rootPath, str);

    // Load the settings
    // If this is the first time, set the path settings based on the root storage path
    if (!Settings::load(path + "/noods/noods.ini"))
    {
        Settings::setBios7Path(path + "/noods/bios7.bin");
        Settings::setBios9Path(path + "/noods/bios9.bin");
        Settings::setFirmwarePath(path + "/noods/firmware.bin");
        Settings::setGbaBiosPath(path + "/noods/gba_bios.bin");
        Settings::save(path + "/noods/noods.ini");
    }
}

extern "C" JNIEXPORT jint JNICALL Java_com_hydra_noods_FileBrowser_loadRom(JNIEnv* env, jobject obj, jstring romPath)
{
    // Convert the Java string to a C++ string
    const char *str = env->GetStringUTFChars(romPath, nullptr);
    std::string path = str;
    env->ReleaseStringUTFChars(romPath, str);

    // Attempt to load the ROM, and return the error code if loading failed
    try
    {
        core = new Core(path, (path.find(".gba", path.length() - 4) != std::string::npos));
        return 0;
    }
    catch (int e)
    {
        return e;
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooView_copyFramebuffer(JNIEnv *env, jobject obj, jobject bitmap)
{
    // Get a new frame if one is ready
    uint32_t *framebuffer = core->gpu.getFrame(false);
    if (!framebuffer) return;

    // Copy the frame to the bitmap
    uint32_t *data;
    AndroidBitmap_lockPixels(env, bitmap, (void**)&data);
    memcpy(data, framebuffer, 256 * 192 * 2 * sizeof(uint32_t));
    AndroidBitmap_unlockPixels(env, bitmap);

    delete[] framebuffer;
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

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_runFrame(JNIEnv *env, jobject obj)
{
    core->runFrame();
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooButton_pressKey(JNIEnv *env, jobject obj, jint key)
{
    core->input.pressKey(key);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooButton_releaseKey(JNIEnv *env, jobject obj, jint key)
{
    core->input.releaseKey(key);
}

extern "C" JNIEXPORT void JNICALL Java_com_hydra_noods_NooActivity_writeSave(JNIEnv *env, jobject obj)
{
    core->cartridge.writeSave();
}
