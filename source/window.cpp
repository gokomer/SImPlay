// Copyright (c) 2022-2025 tsl0922. All rights reserved.
// SPDX-License-Identifier: GPL-2.0-only

#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <strnatcmp.h>
#ifdef _WIN32
#include <windowsx.h>
#endif
#include "theme.h"
#include "window.h"

namespace ImPlay {
Window::Window(Config* config) : Player(config) {
  initSDL();
  window = SDL_CreateWindow(PLAYER_NAME, 1280, 720,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE |
                                SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (window == nullptr) throw std::runtime_error(fmt::format("Failed to create window: {}", SDL_GetError()));
  glContext = SDL_GL_CreateContext(window);
  if (glContext == nullptr) throw std::runtime_error(fmt::format("Failed to create GL context: {}", SDL_GetError()));
  SDL_GL_MakeCurrent(window, glContext);
#ifdef _WIN32
  hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
  if (SUCCEEDED(OleInitialize(nullptr))) oleOk = true;
#endif

  wakeupEventType = SDL_RegisterEvents(1);
  initGui();
  ImGui_ImplSDL3_InitForOpenGL(window, glContext);
}

Window::~Window() {
  ImGui_ImplSDL3_Shutdown();
  exitGui();
#ifdef _WIN32
  if (taskbarList != nullptr) taskbarList->Release();
  if (oleOk) OleUninitialize();
#endif

  SDL_GL_DestroyContext(glContext);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void Window::initSDL() {
  if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(fmt::format("Failed to initialize SDL: {}", SDL_GetError()));

#if defined(IMGUI_IMPL_OPENGL_ES3)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#elif defined(__APPLE__)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
  SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
}

bool Window::init(OptionParser& parser) {
  mpv->wakeupCb() = [this](Mpv* ctx) { wakeup(); };
  mpv->updateCb() = [this](Mpv* ctx) { videoWaiter.notify(); };
  if (!Player::init(parser.options)) return false;

  for (auto& path : parser.paths) {
    if (path == "-") mpv->property("input-terminal", "yes");
    mpv->commandv("loadfile", path.c_str(), "append-play", nullptr);
  }

#ifdef _WIN32
  if (oleOk) setupWin32Taskbar();
  wndProcOld = (WNDPROC)::GetWindowLongPtr(hwnd, GWLP_WNDPROC);
  ::SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(wndProc));
  ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

  mpv->observeProperty<int, MPV_FORMAT_FLAG>("border", [this](int flag) {
    borderless = !static_cast<bool>(flag);
    if (borderless) {
      DWORD style = ::GetWindowLong(hwnd, GWL_STYLE);
      ::SetWindowLong(hwnd, GWL_STYLE, style | WS_CAPTION | WS_MAXIMIZEBOX | WS_THICKFRAME);
    }
    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
  });
#endif
  return true;
}

void Window::run() {
  bool shutdown = false;
  std::thread videoRenderer([&]() {
    while (!shutdown) {
      videoWaiter.wait();
      if (shutdown) break;

      if (mpv->wantRender()) {
        renderVideo();
        wakeup();
      }
    }
  });

  restoreState();
  SDL_ShowWindow(window);

  double lastTime = SDL_GetTicks() / 1000.0;
  while (!shouldClose) {
    SDL_Event event;
    if (minimized) {
      SDL_WaitEvent(nullptr);
      while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        processEvent(event);
      }
    } else {
      while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        processEvent(event);
      }
    }

    mpv->waitEvent();

    render();
    updateCursor();

    double targetDelta = 1.0 / config->Data.Interface.Fps;
    double delta = lastTime - SDL_GetTicks() / 1000.0;
    if (delta > 0 && delta < targetDelta)
      SDL_WaitEventTimeout(nullptr, (Sint32)(delta * 1000));
    else
      lastTime = SDL_GetTicks() / 1000.0;
    lastTime += targetDelta;
  }

  shutdown = true;
  videoWaiter.notify();
  videoRenderer.join();

  saveState();
}

void Window::wakeup() {
  SDL_Event ev{};
  ev.type = wakeupEventType;
  SDL_PushEvent(&ev);
}

void Window::updateCursor() {
  if (!ownCursor || mpv->cursorAutohide == "" || ImGui::GetIO().WantCaptureMouse || ImGui::IsMouseDragging(0)) return;

  bool cursor = true;
  if (mpv->cursorAutohide == "no")
    cursor = true;
  else if (mpv->cursorAutohide == "always")
    cursor = false;
  else
    cursor = (SDL_GetTicks() - lastInputAt) < (Uint64)std::stoi(mpv->cursorAutohide);
  if (cursor)
    SDL_ShowCursor();
  else
    SDL_HideCursor();
  ImGui::SetMouseCursor(cursor ? ImGuiMouseCursor_Arrow : ImGuiMouseCursor_None);
}

void Window::processEvent(const SDL_Event& event) {
  switch (event.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      shutdown();
      shouldClose = true;
      break;
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_MOVED:
      render();
      break;
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
      config->Data.Interface.Scale = SDL_GetWindowDisplayScale(window);
      config->FontReload = true;
      break;
    case SDL_EVENT_WINDOW_MINIMIZED:
      minimized = true;
      break;
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_SHOWN:
      minimized = false;
      break;
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
      ownCursor = true;
      break;
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
      ownCursor = false;
      break;
    case SDL_EVENT_MOUSE_MOTION:
      lastInputAt = SDL_GetTicks();
      if (!ImGui::GetIO().WantCaptureMouse) onCursorEvent(event.motion.x, event.motion.y);
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      lastInputAt = SDL_GetTicks();
      if (!ImGui::GetIO().WantCaptureMouse)
        handleMouse(event.button.button, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN,
                    (SDL_Keymod)SDL_GetModState());
      break;
    case SDL_EVENT_MOUSE_WHEEL:
      lastInputAt = SDL_GetTicks();
      if (!ImGui::GetIO().WantCaptureMouse) onScrollEvent(-event.wheel.x, event.wheel.y);
      break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      lastInputAt = SDL_GetTicks();
      if (!ImGui::GetIO().WantCaptureKeyboard)
        handleKey(event.key.scancode, event.type == SDL_EVENT_KEY_DOWN, (SDL_Keymod)event.key.mod);
      break;
    case SDL_EVENT_DROP_FILE:
      if (!ImGui::GetIO().WantCaptureMouse && event.drop.data != nullptr && event.drop.data[0] != '\0') {
        const char* paths[] = {event.drop.data};
        onDropEvent(1, paths);
      }
      break;
    default:
      break;
  }
}

void Window::handleKey(SDL_Scancode scancode, bool down, SDL_Keymod mods) {
  std::string name;
  if (mods & SDL_KMOD_SHIFT) {
    if (auto s = shiftMappings.find(scancode); s != shiftMappings.end()) {
      name = s->second;
      mods = (SDL_Keymod)(mods & ~SDL_KMOD_SHIFT);
    }
  }
  if (name.empty()) {
    auto s = keyMappings.find(scancode);
    if (s == keyMappings.end()) return;
    name = s->second;
  }

  std::vector<std::string> keys;
  translateMod(keys, mods);
  keys.push_back(name);
  sendKeyEvent(fmt::format("{}", join(keys, "+")), down);
}

void Window::handleMouse(Uint8 button, bool down, SDL_Keymod mods) {
  std::vector<std::string> keys;
  translateMod(keys, mods);
  auto s = mbtnMappings.find(button);
  if (s == mbtnMappings.end()) return;
  keys.push_back(s->second);
  sendKeyEvent(fmt::format("{}", join(keys, "+")), down);
}

void Window::sendKeyEvent(std::string key, bool action) {
  if (action)
    onKeyDownEvent(key);
  else
    onKeyUpEvent(key);
}

void Window::translateMod(std::vector<std::string>& keys, SDL_Keymod mods) {
  if (mods & SDL_KMOD_CTRL) keys.emplace_back("Ctrl");
  if (mods & SDL_KMOD_ALT) keys.emplace_back("Alt");
  if (mods & SDL_KMOD_SHIFT) keys.emplace_back("Shift");
  if (mods & SDL_KMOD_GUI) keys.emplace_back("Meta");
}

#ifdef _WIN32
int64_t Window::GetWid() { return config->Data.Mpv.UseWid ? (int64_t)(intptr_t)hwnd : 0; }
#endif

GLAddrLoadFunc Window::GetGLAddrFunc() { return (GLAddrLoadFunc)SDL_GL_GetProcAddress; }

std::string Window::GetClipboardString() {
  auto* s = SDL_GetClipboardText();
  std::string r = s ? s : "";
  SDL_free(s);
  return r;
}

void Window::GetMonitorSize(int* w, int* h) {
  const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(getDisplay());
  *w = mode->w;
  *h = mode->h;
}

int Window::GetMonitorRefreshRate() {
  const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(getDisplay());
  return (int)mode->refresh_rate;
}

void Window::GetFramebufferSize(int* w, int* h) { SDL_GetWindowSizeInPixels(window, w, h); }

void Window::MakeContextCurrent() { SDL_GL_MakeCurrent(window, glContext); }

void Window::DeleteContext() { SDL_GL_MakeCurrent(window, nullptr); }

void Window::SwapBuffers() { SDL_GL_SwapWindow(window); }

void Window::SetSwapInterval(int interval) { SDL_GL_SetSwapInterval(interval); }

void Window::BackendNewFrame() { ImGui_ImplSDL3_NewFrame(); }

void Window::GetWindowScale(float* x, float* y) {
  float s = SDL_GetWindowDisplayScale(window);
  *x = s;
  *y = s;
}

void Window::GetWindowPos(int* x, int* y) { SDL_GetWindowPosition(window, x, y); }

void Window::SetWindowPos(int x, int y) { SDL_SetWindowPosition(window, x, y); }

void Window::GetWindowSize(int* w, int* h) { SDL_GetWindowSize(window, w, h); }

void Window::SetWindowSize(int w, int h) { SDL_SetWindowSize(window, w, h); }

void Window::SetWindowTitle(std::string title) { SDL_SetWindowTitle(window, title.c_str()); }

void Window::SetWindowAspectRatio(int num, int den) {
  SDL_SetWindowAspectRatio(window, (float)num / den, (float)num / den);
}

void Window::SetWindowMaximized(bool m) {
  if (m)
    SDL_MaximizeWindow(window);
  else
    SDL_RestoreWindow(window);
}

void Window::SetWindowMinimized(bool m) {
  if (m)
    SDL_MinimizeWindow(window);
  else
    SDL_RestoreWindow(window);
}

void Window::SetWindowDecorated(bool d) { SDL_SetWindowBordered(window, d); }

void Window::SetWindowFloating(bool f) { SDL_SetWindowAlwaysOnTop(window, f); }

void Window::SetWindowFullscreen(bool fs) { SDL_SetWindowFullscreen(window, fs); }

void Window::SetWindowShouldClose(bool c) { if (c) shouldClose = true; }

SDL_DisplayID Window::getDisplay() { return SDL_GetDisplayForWindow(window); }

#ifdef _WIN32
void Window::setupWin32Taskbar() {
  if (FAILED(CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL, IID_ITaskbarList3, (void**)&taskbarList))) return;
  if (FAILED(taskbarList->HrInit())) {
    taskbarList->Release();
    taskbarList = nullptr;
    return;
  }
  mpv->observeEvent(MPV_EVENT_START_FILE, [this](void*) { taskbarList->SetProgressState(hwnd, TBPF_NORMAL); });
  mpv->observeEvent(MPV_EVENT_END_FILE, [this](void*) { taskbarList->SetProgressState(hwnd, TBPF_NOPROGRESS); });
  mpv->observeProperty<int64_t, MPV_FORMAT_INT64>("percent-pos", [this](int64_t pos) {
    if (pos > 0) taskbarList->SetProgressValue(hwnd, pos, 100);
  });
}

// borderless window: https://github.com/rossy/borderless-window
LRESULT CALLBACK Window::wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  auto win = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
  switch (uMsg) {
    case WM_NCLBUTTONDOWN:
    case WM_NCRBUTTONDOWN:
    case WM_NCMBUTTONDOWN:
    case WM_NCXBUTTONDOWN:
      if (win->config->Data.Mpv.UseWid && ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopup)) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddFocusEvent(false); // may close popup
      }
      break;
    case WM_NCACTIVATE:
    case WM_NCPAINT:
      if (win->borderless) return DefWindowProcW(hWnd, uMsg, wParam, lParam);
      break;
    case WM_NCCALCSIZE: {
      if (!win->borderless) break;

      RECT& rect = *reinterpret_cast<RECT*>(lParam);
      RECT client = rect;

      DefWindowProcW(hWnd, uMsg, wParam, lParam);

      if (IsMaximized(hWnd)) {
        HMONITOR mon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = {.cbSize = sizeof mi};
        GetMonitorInfoW(mon, &mi);
        rect = mi.rcWork;
      } else {
        rect = client;
      }

      return 0;
    }
    case WM_NCHITTEST: {
      if (!win->borderless) break;
      if (IsMaximized(hWnd)) return HTCLIENT;

      POINT mouse = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      ScreenToClient(hWnd, &mouse);
      RECT client;
      GetClientRect(hWnd, &client);

      int frame_size = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
      int diagonal_width = frame_size * 2 + GetSystemMetrics(SM_CXBORDER);

      if (mouse.y < frame_size) {
        if (mouse.x < diagonal_width) return HTTOPLEFT;
        if (mouse.x >= client.right - diagonal_width) return HTTOPRIGHT;
        return HTTOP;
      }

      if (mouse.y >= client.bottom - frame_size) {
        if (mouse.x < diagonal_width) return HTBOTTOMLEFT;
        if (mouse.x >= client.right - diagonal_width) return HTBOTTOMRIGHT;
        return HTBOTTOM;
      }

      if (mouse.x < frame_size) return HTLEFT;
      if (mouse.x >= client.right - frame_size) return HTRIGHT;

      return HTCLIENT;
    } break;
  }
  return ::CallWindowProc(win->wndProcOld, hWnd, uMsg, wParam, lParam);
}
#endif

void Window::Waiter::wait() {
  std::unique_lock<std::mutex> l(lock);
  cond.wait(l, [this] { return notified; });
  notified = false;
}

void Window::Waiter::wait_until(std::chrono::steady_clock::time_point time) {
  std::unique_lock<std::mutex> l(lock);
  cond.wait_until(l, time, [this] { return notified; });
  notified = false;
}

void Window::Waiter::notify() {
  {
    std::lock_guard<std::mutex> l(lock);
    notified = true;
  }
  cond.notify_one();
}
}  // namespace ImPlay