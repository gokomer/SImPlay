// Copyright (c) 2022-2025 tsl0922. All rights reserved.
// SPDX-License-Identifier: GPL-2.0-only

#pragma once
#include "player.h"
#ifdef _WIN32
#include <ole2.h>
#include <shobjidl.h>
#endif
#include <SDL3/SDL.h>
#include <condition_variable>

namespace ImPlay {
class Window : Player {
 public:
  explicit Window(Config *config);
  ~Window();

  bool init(OptionParser &parser);
  void run();

 private:
  void initSDL();
  void wakeup();
  void updateCursor();
  void processEvent(const SDL_Event &event);

  void handleKey(SDL_Scancode scancode, bool down, SDL_Keymod mods);
  void handleMouse(Uint8 button, bool down, SDL_Keymod mods);

  void sendKeyEvent(std::string key, bool action);
  void translateMod(std::vector<std::string> &keys, SDL_Keymod mods);

  SDL_DisplayID getDisplay();

#ifdef _WIN32
  int64_t GetWid() override;
#endif
  GLAddrLoadFunc GetGLAddrFunc() override;
  std::string GetClipboardString() override;
  void GetMonitorSize(int *w, int *h) override;
  int GetMonitorRefreshRate() override;
  void GetFramebufferSize(int *w, int *h) override;
  void MakeContextCurrent() override;
  void DeleteContext() override;
  void SwapBuffers() override;
  void SetSwapInterval(int interval) override;
  void BackendNewFrame() override;
  void GetWindowScale(float *x, float *y) override;
  void GetWindowPos(int *x, int *y) override;
  void SetWindowPos(int x, int y) override;
  void GetWindowSize(int *w, int *h) override;
  void SetWindowSize(int w, int h) override;
  void SetWindowTitle(std::string title) override;
  void SetWindowAspectRatio(int num, int den) override;
  void SetWindowMaximized(bool m) override;
  void SetWindowMinimized(bool m) override;
  void SetWindowDecorated(bool d) override;
  void SetWindowFloating(bool f) override;
  void SetWindowFullscreen(bool fs) override;
  void SetWindowShouldClose(bool c) override;

  SDL_Window *window = nullptr;
  SDL_GLContext glContext = nullptr;
  bool ownCursor = true;
  bool minimized = false;
  bool shouldClose = false;
  Uint64 lastInputAt = 0;
  Uint32 wakeupEventType = 0;
#ifdef _WIN32
  bool borderless = false;
  bool oleOk = false;
  HWND hwnd;
  WNDPROC wndProcOld = nullptr;
  ITaskbarList3 *taskbarList = nullptr;

  void setupWin32Taskbar();
  static LRESULT CALLBACK wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

  struct Waiter {
   public:
    void wait();
    void wait_until(std::chrono::steady_clock::time_point time);
    void notify();

   private:
    std::mutex lock;
    std::condition_variable cond;
    bool notified = false;
  };

  struct Waiter videoWaiter;

  // clang-format off
  const std::map<SDL_Scancode, std::string> keyMappings = {
      {SDL_SCANCODE_SPACE, "SPACE"}, {SDL_SCANCODE_APOSTROPHE, "'"},
      {SDL_SCANCODE_COMMA, ","}, {SDL_SCANCODE_MINUS, "-"},
      {SDL_SCANCODE_PERIOD, "."}, {SDL_SCANCODE_SLASH, "/"},

      {SDL_SCANCODE_0, "0"}, {SDL_SCANCODE_1, "1"}, {SDL_SCANCODE_2, "2"},
      {SDL_SCANCODE_3, "3"}, {SDL_SCANCODE_4, "4"}, {SDL_SCANCODE_5, "5"},
      {SDL_SCANCODE_6, "6"}, {SDL_SCANCODE_7, "7"}, {SDL_SCANCODE_8, "8"},
      {SDL_SCANCODE_9, "9"},

      {SDL_SCANCODE_SEMICOLON, ";"}, {SDL_SCANCODE_EQUALS, "="},

      {SDL_SCANCODE_A, "a"}, {SDL_SCANCODE_B, "b"}, {SDL_SCANCODE_C, "c"},
      {SDL_SCANCODE_D, "d"}, {SDL_SCANCODE_E, "e"}, {SDL_SCANCODE_F, "f"},
      {SDL_SCANCODE_G, "g"}, {SDL_SCANCODE_H, "h"}, {SDL_SCANCODE_I, "i"},
      {SDL_SCANCODE_J, "j"}, {SDL_SCANCODE_K, "k"}, {SDL_SCANCODE_L, "l"},
      {SDL_SCANCODE_M, "m"}, {SDL_SCANCODE_N, "n"}, {SDL_SCANCODE_O, "o"},
      {SDL_SCANCODE_P, "p"}, {SDL_SCANCODE_Q, "q"}, {SDL_SCANCODE_R, "r"},
      {SDL_SCANCODE_S, "s"}, {SDL_SCANCODE_T, "t"}, {SDL_SCANCODE_U, "u"},
      {SDL_SCANCODE_V, "v"}, {SDL_SCANCODE_W, "w"}, {SDL_SCANCODE_X, "x"},
      {SDL_SCANCODE_Y, "y"}, {SDL_SCANCODE_Z, "z"},

      {SDL_SCANCODE_LEFTBRACKET, "["}, {SDL_SCANCODE_BACKSLASH, "\\"},
      {SDL_SCANCODE_RIGHTBRACKET, "]"}, {SDL_SCANCODE_GRAVE, "`"},
      {SDL_SCANCODE_ESCAPE, "ESC"}, {SDL_SCANCODE_RETURN, "ENTER"},
      {SDL_SCANCODE_TAB, "TAB"}, {SDL_SCANCODE_BACKSPACE, "BS"},
      {SDL_SCANCODE_INSERT, "INS"}, {SDL_SCANCODE_DELETE, "DEL"},
      {SDL_SCANCODE_RIGHT, "RIGHT"}, {SDL_SCANCODE_LEFT, "LEFT"},
      {SDL_SCANCODE_DOWN, "DOWN"}, {SDL_SCANCODE_UP, "UP"},
      {SDL_SCANCODE_PAGEUP, "PGUP"}, {SDL_SCANCODE_PAGEDOWN, "PGDWN"},
      {SDL_SCANCODE_HOME, "HOME"}, {SDL_SCANCODE_END, "END"},
      {SDL_SCANCODE_PRINTSCREEN, "PRINT"}, {SDL_SCANCODE_PAUSE, "PAUSE"},

      {SDL_SCANCODE_F1, "F1"}, {SDL_SCANCODE_F2, "F2"}, {SDL_SCANCODE_F3, "F3"},
      {SDL_SCANCODE_F4, "F4"}, {SDL_SCANCODE_F5, "F5"}, {SDL_SCANCODE_F6, "F6"},
      {SDL_SCANCODE_F7, "F7"}, {SDL_SCANCODE_F8, "F8"}, {SDL_SCANCODE_F9, "F9"},
      {SDL_SCANCODE_F10, "F10"}, {SDL_SCANCODE_F11, "F11"}, {SDL_SCANCODE_F12, "F12"},
      {SDL_SCANCODE_F13, "F13"}, {SDL_SCANCODE_F14, "F14"}, {SDL_SCANCODE_F15, "F15"},
      {SDL_SCANCODE_F16, "F16"}, {SDL_SCANCODE_F17, "F17"}, {SDL_SCANCODE_F18, "F18"},
      {SDL_SCANCODE_F19, "F19"}, {SDL_SCANCODE_F20, "F20"}, {SDL_SCANCODE_F21, "F21"},
      {SDL_SCANCODE_F22, "F22"}, {SDL_SCANCODE_F23, "F23"}, {SDL_SCANCODE_F24, "F24"},

      {SDL_SCANCODE_KP_0, "KP0"}, {SDL_SCANCODE_KP_1, "KP1"}, {SDL_SCANCODE_KP_2, "KP2"},
      {SDL_SCANCODE_KP_3, "KP3"}, {SDL_SCANCODE_KP_4, "KP4"}, {SDL_SCANCODE_KP_5, "KP5"},
      {SDL_SCANCODE_KP_6, "KP6"}, {SDL_SCANCODE_KP_7, "KP7"}, {SDL_SCANCODE_KP_8, "KP8"},
      {SDL_SCANCODE_KP_9, "KP9"}, {SDL_SCANCODE_KP_ENTER, "KP_ENTER"},
  };

  const std::map<SDL_Scancode, std::string> shiftMappings = {
      {SDL_SCANCODE_0, ")"}, {SDL_SCANCODE_1, "!"}, {SDL_SCANCODE_2, "@"},
      {SDL_SCANCODE_3, "#"}, {SDL_SCANCODE_4, "$"}, {SDL_SCANCODE_5, "%"},
      {SDL_SCANCODE_6, "^"}, {SDL_SCANCODE_7, "&"}, {SDL_SCANCODE_8, "*"},
      {SDL_SCANCODE_9, "("}, {SDL_SCANCODE_MINUS, "_"}, {SDL_SCANCODE_EQUALS, "+"},
      {SDL_SCANCODE_LEFTBRACKET, "{"}, {SDL_SCANCODE_RIGHTBRACKET, "}"},
      {SDL_SCANCODE_BACKSLASH, "|"}, {SDL_SCANCODE_SEMICOLON, ":"},
      {SDL_SCANCODE_APOSTROPHE, "\""}, {SDL_SCANCODE_COMMA, "<"},
      {SDL_SCANCODE_PERIOD, ">"}, {SDL_SCANCODE_SLASH, "?"},
  };

  const std::map<Uint8, std::string> mbtnMappings = {
      {SDL_BUTTON_LEFT, "MBTN_LEFT"}, {SDL_BUTTON_MIDDLE, "MBTN_MID"},
      {SDL_BUTTON_RIGHT, "MBTN_RIGHT"}, {SDL_BUTTON_X1, "MP_MBTN_BACK"},
      {SDL_BUTTON_X2, "MP_MBTN_FORWARD"},
  };
  // clang-format on
};
}  // namespace ImPlay