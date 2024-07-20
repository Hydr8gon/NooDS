#include <cstdarg>
#include <algorithm>

#include <fcntl.h>
#include <fstream>

#include "libretro.h"

#include "../common/screen_layout.h"
#include "../settings.h"
#include "../core.h"
#include "../defines.h"

#ifndef VERSION
#define VERSION "0.1"
#endif

static retro_environment_t envCallback;
static retro_video_refresh_t videoCallback;
static retro_audio_sample_batch_t audioBatchCallback;
static retro_input_poll_t inputPollCallback;
static retro_input_state_t inputStateCallback;

static struct retro_log_callback logging;
static retro_log_printf_t logCallback;

static std::string systemPath;
static std::string savesPath;

static Core *core;
static ScreenLayout layout;

static std::string ndsPath;
static std::string gbaPath;

static int ndsSaveFd = -1;
static int gbaSaveFd = -1;

static std::string touchMode;
static std::string screenSwapMode;

static int screenArrangement;
static bool renderTopScreen;
static bool renderBotScreen;

static bool showTouchCursor;
static bool screenSwapped;
static bool showBottomScreen;
static bool screenTouched;

static int lastMouseX = 0;
static int lastMouseY = 0;

static int touchX = 0;
static int touchY = 0;

static int keymap[] = {
  RETRO_DEVICE_ID_JOYPAD_A,
  RETRO_DEVICE_ID_JOYPAD_B,
  RETRO_DEVICE_ID_JOYPAD_SELECT,
  RETRO_DEVICE_ID_JOYPAD_START,
  RETRO_DEVICE_ID_JOYPAD_RIGHT,
  RETRO_DEVICE_ID_JOYPAD_LEFT,
  RETRO_DEVICE_ID_JOYPAD_UP,
  RETRO_DEVICE_ID_JOYPAD_DOWN,
  RETRO_DEVICE_ID_JOYPAD_R,
  RETRO_DEVICE_ID_JOYPAD_L,
  RETRO_DEVICE_ID_JOYPAD_X,
  RETRO_DEVICE_ID_JOYPAD_Y
};

static int32_t clampValue(int32_t value, int32_t min, int32_t max)
{
  return std::max(min, std::min(max, value));
}

static std::string normalizePath(std::string path, bool addSlash = false)
{
  std::string normalizedPath = path;
  if (addSlash && normalizedPath.back() != '/') {
    normalizedPath += '/';
  }
  return normalizedPath;
}

static std::string getNameFromPath(std::string path) {
  std::string base = path.substr(path.find_last_of("/\\") + 1);
  return base.substr(0, base.rfind("."));
}

static void logFallback(enum retro_log_level level, const char *fmt, ...)
{
  (void)level;
  va_list va;
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
}

static std::string fetchVariable(std::string key, std::string def)
{
  struct retro_variable var = { nullptr };
  var.key = key.c_str();

  if (!envCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || var.value == nullptr)
  {
    logCallback(RETRO_LOG_WARN, "Fetching variable %s failed.", var.key);
    return def;
  }

  return std::string(var.value);
}

static bool fetchVariableBool(std::string key, bool def)
{
  return fetchVariable(key, def ? "enabled" : "disabled") == "enabled";
}

static int fetchVariableEnum(std::string key, std::vector<std::string> list, int def = 0)
{
  auto val = fetchVariable(key, list[def]);
  auto itr = std::find(list.begin(), list.end(), val);

  return std::distance(list.begin(), itr);
}

static std::string getSaveDir()
{
  char* dir = nullptr;
  if (!envCallback(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir) || dir == nullptr)
  {
    logCallback(RETRO_LOG_INFO, "No save directory provided by LibRetro.");
    return std::string("NooDS");
  }
  return std::string(dir);
}

static std::string getSystemDir()
{
  char* dir = nullptr;
  if (!envCallback(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) || dir == nullptr)
  {
    logCallback(RETRO_LOG_INFO, "No system directory provided by LibRetro.");
    return std::string("NooDS");
  }
  return std::string(dir);
}

static bool getButtonState(unsigned id)
{
  return inputStateCallback(0, RETRO_DEVICE_JOYPAD, 0, id);
}

static float getAxisState(unsigned index, unsigned id)
{
  return inputStateCallback(0, RETRO_DEVICE_ANALOG, index, id);
}

static void initInput(void)
{
  static const struct retro_controller_description controllers[] = {
    { "Nintendo DS", RETRO_DEVICE_JOYPAD },
    { NULL, 0 },
  };

  static const struct retro_controller_info ports[] = {
    { controllers, 1 },
    { NULL, 0 },
  };

  envCallback(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

  struct retro_input_descriptor desc[] = {
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "Swap screens" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "Touch joystick" },
    { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "Touch joystick X" },
    { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "Touch joystick Y" },
    { 0 },
  };

  envCallback(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &desc);
}

static void initConfig()
{
  static const retro_variable values[] = {
    { "noods_directBoot", "Direct Boot; enabled|disabled" },
    { "noods_fpsLimiter", "FPS Limiter; Disabled|Light|Accurate" },
    { "noods_threaded2D", "Threaded 2D; enabled|disabled" },
    { "noods_threaded3D", "Threaded 3D; Disabled|1 Thread|2 Threads|3 Threads" },
    { "noods_highRes3D", "High Resolution 3D; disabled|enabled" },
    { "noods_screenArrangement", "Screen Arrangement; Vertical|Horizontal|Single Screen" },
    { "noods_screenFilter", "Screen Filter; Linear|Nearest|Upscaled" },
    { "noods_screenGhost", "Simulate Ghosting; disabled|enabled" },
    { "noods_swapScreenMode", "Swap Screen Mode; Toggle|Hold" },
    { "noods_touchMode", "Touch Mode; Auto|Pointer|Joystick|None" },
    { "noods_touchCursor", "Show Touch Cursor; enabled|disabled" },
    { nullptr, nullptr }
  };

  envCallback(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)values);
}

static void updateConfig()
{
  Settings::bios9Path = systemPath + "bios9.bin";
  Settings::bios7Path = systemPath + "bios7.bin";
  Settings::firmwarePath = systemPath + "firmware.bin";
  Settings::sdImagePath = systemPath + "nds_sd_card.bin";

  Settings::directBoot = fetchVariableBool("noods_directBoot", true);
  Settings::fpsLimiter = fetchVariableEnum("noods_fpsLimiter", {"Disabled", "Light", "Accurate"});
  Settings::threaded2D = fetchVariableBool("noods_threaded2D", true);
  Settings::threaded3D = fetchVariableEnum("noods_threaded3D", {"Disabled", "1 Thread", "2 Threads", "3 Threads"});
  Settings::highRes3D = fetchVariableBool("noods_highRes3D", false);
  Settings::screenFilter = fetchVariableEnum("noods_screenFilter", {"Nearest", "Upscaled", "Linear"}, 2);
  Settings::screenGhost = fetchVariableBool("noods_screenGhost", false);

  screenArrangement = fetchVariableEnum("noods_screenArrangement", {"Vertical", "Horizontal", "Single Screen"});
  screenSwapMode = fetchVariable("noods_swapScreenMode", "Toggle");
  touchMode = fetchVariable("noods_touchMode", "Touch");
  showTouchCursor = fetchVariableBool("noods_touchCursor", true);

  ScreenLayout::screenArrangement = screenArrangement + 1;
  layout.update(0, 0, false, false);
}

static void checkConfigVariables()
{
  bool updated = false;
  envCallback(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated);

  if (updated)
  {
    updateConfig();

    retro_system_av_info info;
    retro_get_system_av_info(&info);
    envCallback(RETRO_ENVIRONMENT_SET_GEOMETRY, &info);
  }
}

static void updateScreenState()
{
  bool singleScreen = ScreenLayout::screenArrangement == 3;
  bool bottomScreen = singleScreen && showBottomScreen;

  renderTopScreen = !bottomScreen && (singleScreen || ScreenLayout::screenSizing < 2);
  renderBotScreen = !singleScreen || (bottomScreen || ScreenLayout::screenSizing == 2);
}

static void drawCursor(uint32_t *data, int32_t posX, int32_t posY)
{
  bool shift = Settings::highRes3D || Settings::screenFilter == 1;

  uint32_t minX = layout.botX << shift;
  uint32_t maxX = layout.minWidth << shift;

  uint32_t minY = layout.botY << shift;
  uint32_t maxY = layout.minHeight << shift;

  uint32_t curX = (layout.botX + posX) << shift;
  uint32_t curY = (layout.botY + posY) << shift;

  uint32_t cursorSize = 2 << shift;
  uint32_t* baseOffset = (uint32_t*)data;

  uint32_t startY = clampValue(curY - cursorSize, minY, maxY);
  uint32_t endY = clampValue(curY + cursorSize, minY, maxY);

  uint32_t startX = clampValue(curX - cursorSize, minX, maxX);
  uint32_t endX = clampValue(curX + cursorSize, minX, maxX);

  for (uint32_t y = startY; y < endY; y++)
  {
    for (uint32_t x = startX; x < endX; x++)
    {
      uint32_t* offset = baseOffset + (y * maxX) + x;
      uint32_t pixel = *offset;
      *(uint32_t*)offset = (0xFFFFFF - pixel) | 0xFF000000;
    }
  }
}

static void copyScreen(uint32_t *src,  uint32_t *dst, int sx, int sy, int width, int height, int stride)
{
  for (int y = 0; y < height; y++)
  {
    uint32_t* srcPtr = &src[y * width];
    uint32_t* dstPtr = &dst[(sy + y) * stride + sx];

    for (int x = 0; x < width; x++)
    {
      *dstPtr =
        ((*srcPtr & 0xFF000000)) |
        ((*srcPtr & 0x00FF0000) >> 16) |
        ((*srcPtr & 0x0000FF00)) |
        ((*srcPtr & 0x000000FF) << 16);

      dstPtr++;
      srcPtr++;
    }
  }
}

static void drawTexture(uint32_t *buffer)
{
  bool shift = Settings::highRes3D || Settings::screenFilter == 1;
  auto width = layout.minWidth << shift;
  auto height = layout.minHeight << shift;

  uint32_t* bufferPtr = new uint32_t[width * height]();

  if (renderTopScreen)
  {
    copyScreen(
      &buffer[0], bufferPtr,
      layout.topX << shift, layout.topY << shift,
      layout.topWidth << shift, layout.topHeight << shift,
      width
    );
  }

  if (renderBotScreen)
  {
    copyScreen(
      &buffer[(256 * 192) << (shift * 2)], bufferPtr,
      layout.botX << shift, layout.botY << shift,
      layout.botWidth << shift, layout.botHeight << shift,
      width
    );

    if (showTouchCursor)
      drawCursor(bufferPtr, touchX, touchY);
  }

  videoCallback(bufferPtr, width, height, width * 4);
  delete[] bufferPtr;
}

static void playbackAudio(void)
{
  static int16_t buffer[547 * 2];
  uint32_t *original = core->spu.getSamples(547);

  for (int i = 0; i < 547; i++)
  {
    buffer[i * 2 + 0] = original[i] >>  0;
    buffer[i * 2 + 1] = original[i] >> 16;
  }
  delete[] original;

  uint32_t size = sizeof(buffer) / (2 * sizeof(int16_t));
  audioBatchCallback(buffer, size);
}

static int getSaveFileDesc(std::string path)
{
  int fd = open(path.c_str(), O_RDWR);
  if (fd == -1) {
    std::ofstream file(path, std::ios::binary);
    if (file.is_open()) {
      file.put(0xFF);
      file.close();
    }
    fd = open(path.c_str(), O_RDWR);
  }
  return fd;
}

static void closeSaveFileDesc()
{
  close(ndsSaveFd);
  ndsSaveFd = -1;

  close(gbaSaveFd);
  gbaSaveFd = -1;
}

static bool createCore(std::string ndsRom = "", std::string gbaRom = "")
{
  int ndsGameFd = -1;
  int gbaGameFd = -1;

  closeSaveFileDesc();

  try
  {
    if (core) delete core;

    if (ndsRom != "")
    {
      ndsGameFd = open(ndsRom.c_str(), O_RDONLY);
      ndsSaveFd = getSaveFileDesc(savesPath + getNameFromPath(ndsRom) + ".sav");
    }

    if (gbaRom != "")
    {
      ndsGameFd = open(ndsRom.c_str(), O_RDONLY);
      gbaSaveFd = getSaveFileDesc(savesPath + getNameFromPath(gbaRom) + ".sav");
    }

    core = new Core("", "", 0, ndsGameFd, gbaGameFd, ndsSaveFd, gbaSaveFd);

    close(ndsGameFd);
    close(gbaGameFd);

    return true;
  }
  catch (CoreError e)
  {
    switch (e)
    {
      case ERROR_BIOS:
        logCallback(RETRO_LOG_INFO, "Error Loading BIOS");
        break;
      case ERROR_FIRM:
        logCallback(RETRO_LOG_INFO, "Error Loading Firmware");
        break;
      case ERROR_ROM:
        logCallback(RETRO_LOG_INFO, "Error Loading ROM");
        break;
    }

    close(ndsGameFd);
    close(gbaGameFd);

    closeSaveFileDesc();

    core = nullptr;
    return false;
  }
}

void retro_get_system_info(retro_system_info* info)
{
  info->need_fullpath = true;
  info->valid_extensions = "nds";
  info->library_version = VERSION;
  info->library_name = "NooDS";
  info->block_extract = true;
}

void retro_get_system_av_info(retro_system_av_info* info)
{
  info->geometry.base_width = layout.minWidth;
  info->geometry.base_height = layout.minHeight;

  info->geometry.max_width = info->geometry.base_width;
  info->geometry.max_height = info->geometry.base_height;
  info->geometry.aspect_ratio = (float)layout.minWidth / (float)layout.minHeight;

  info->timing.fps = 32.0f * 1024.0f * 1024.0f / 560190.0f;
  info->timing.sample_rate = 32.0f * 1024.0f;
}

void retro_set_environment(retro_environment_t cb)
{
  bool nogameSupport = true;
  cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &nogameSupport);

  envCallback = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
  videoCallback = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
  audioBatchCallback = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
}

void retro_set_input_poll(retro_input_poll_t cb)
{
  inputPollCallback = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
  inputStateCallback = cb;
}

void retro_init(void)
{
  enum retro_pixel_format xrgb888 = RETRO_PIXEL_FORMAT_XRGB8888;
  envCallback(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &xrgb888);

  if (envCallback(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
    logCallback = logging.log;
  else
    logCallback = logFallback;

  systemPath = normalizePath(getSystemDir(), true);
  savesPath = normalizePath(getSaveDir(), true);
}

void retro_deinit(void)
{
  logCallback = nullptr;
}

bool retro_load_game(const struct retro_game_info* game)
{
  ndsPath = "";
  gbaPath = "";

  if (game && game->path)
  {
    std::string path = game->path;

    if (path.find(".nds", path.length() - 4) != std::string::npos)
      ndsPath = path;
    else if (path.find(".gba", path.length() - 4) != std::string::npos)
      gbaPath = path;
    else
      return false;
  }

  initConfig();
  updateConfig();

  initInput();
  updateScreenState();

  if (createCore(ndsPath, gbaPath))
  {
    core->cartridgeNds.writeSave();
    core->cartridgeGba.writeSave();

    return true;
  }

  return false;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info* info, size_t num)
{
  return false;
}

void retro_unload_game(void)
{
  if (core) {
    core->cartridgeNds.writeSave();
    core->cartridgeGba.writeSave();

    delete core;
  }

  closeSaveFileDesc();
}

void retro_reset(void)
{
  createCore(ndsPath, gbaPath);
}

void retro_run(void)
{
  checkConfigVariables();
  inputPollCallback();

  for (int i = 0; i < sizeof(keymap) / sizeof(*keymap); ++i)
  {
    if (getButtonState(keymap[i]))
      core->input.pressKey(i);
    else
      core->input.releaseKey(i);
  }

  bool swap = getButtonState(RETRO_DEVICE_ID_JOYPAD_R2);

  if (screenSwapped != swap) {
    if (screenSwapMode == "Toggle" && swap)
      showBottomScreen = !showBottomScreen;

    if (screenSwapMode == "Hold")
      showBottomScreen = swap;

    screenSwapped = swap;
  }

  updateScreenState();

  if (renderBotScreen)
  {
    bool touchScreen = false;
    auto pointerX = touchX;
    auto pointerY = touchY;

    if (touchMode == "Pointer" || touchMode == "Auto")
    {
      auto posX = inputStateCallback(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
      auto posY = inputStateCallback(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);

      auto newX = static_cast<int>((posX + 0x7fff) / (float)(0x7fff * 2) * layout.minWidth);
      auto newY = static_cast<int>((posY + 0x7fff) / (float)(0x7fff * 2) * layout.minHeight);

      bool inScreenX = newX >= layout.botX && newX <= layout.botX + layout.botWidth;
      bool inScreenY = newY >= layout.botY && newY <= layout.botY + layout.botHeight;

      if (inScreenX && inScreenY) {
        touchScreen |= inputStateCallback(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
        touchScreen |= inputStateCallback(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
      }

      if ((posX != 0 || posY != 0) && (lastMouseX != newX || lastMouseY != newY)) {
        lastMouseX = newX;
        lastMouseY = newY;

        pointerX = newX - layout.botX;
        pointerY = newY - layout.botY;
      }
    }

    if (touchMode == "Joystick" || touchMode == "Auto")
    {
      auto speedX = (layout.botWidth / 40.0);
      auto speedY = (layout.botHeight / 40.0);

      float moveX = getAxisState(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
      float moveY = getAxisState(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

      touchScreen |= getButtonState(RETRO_DEVICE_ID_JOYPAD_R3);

      if (moveX != 0 || moveY != 0) {
        pointerX = touchX + static_cast<int>((moveX / 32767) * speedX);
        pointerY = touchY + static_cast<int>((moveY / 32767) * speedY);
      }
    }

    touchX = clampValue(pointerX, 0, layout.botWidth);
    touchY = clampValue(pointerY, 0, layout.botHeight);

    if (touchScreen)
    {
      core->input.pressScreen();
      core->spi.setTouch(touchX, touchY);
      screenTouched = true;
    }
    else if (screenTouched)
    {
      core->input.releaseScreen();
      core->spi.clearTouch();
      screenTouched = false;
    }
  }

  core->runFrame();

  static uint32_t framebuffer[256 * 192 * 8];
  core->gpu.getFrame(framebuffer, false);

  drawTexture(framebuffer);
  playbackAudio();
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
}

size_t retro_serialize_size(void)
{
  FILE* tmpFile = tmpfile();

  int fd = fileno(tmpFile);
  core->saveStates.setFd(fd, false);

  core->saveStates.checkState();
  core->saveStates.saveState();

  fflush(tmpFile);
  fseek(tmpFile, 0, SEEK_END);
  size_t size = ftell(tmpFile);

  fclose(tmpFile);
  close(fd);

  return size;
}

bool retro_serialize(void* data, size_t size)
{
  FILE* tmpFile = tmpfile();

  int fd = fileno(tmpFile);
  core->saveStates.setFd(fd, false);

  core->saveStates.checkState();
  core->saveStates.saveState();

  fflush(tmpFile);
  fseek(tmpFile, 0, SEEK_SET);
  fread(data, 1, size, tmpFile);

  fclose(tmpFile);
  close(fd);

  return true;
}

bool retro_unserialize(const void* data, size_t size)
{
  FILE* tmpFile = tmpfile();

  fwrite(data, 1, size, tmpFile);
  fflush(tmpFile);

  int fd = fileno(tmpFile);
  core->saveStates.setFd(fd, false);

  core->saveStates.checkState();
  core->saveStates.loadState();

  fclose(tmpFile);
  close(fd);

  return true;
}

unsigned retro_get_region(void)
{
  return RETRO_REGION_NTSC;
}

unsigned retro_api_version()
{
  return RETRO_API_VERSION;
}

size_t retro_get_memory_size(unsigned id)
{
  if (id == RETRO_MEMORY_SYSTEM_RAM)
  {
    return 0x400000;
  }
  return 0;
}

void* retro_get_memory_data(unsigned id)
{
  if (id == RETRO_MEMORY_SYSTEM_RAM)
  {
    return 0;
  }
  return NULL;
}

void retro_cheat_set(unsigned index, bool enabled, const char* code)
{
}

void retro_cheat_reset(void)
{
}
