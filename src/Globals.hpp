#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/helpers/time/Time.hpp>

inline HANDLE pHandle = nullptr;

class CHyprspaceWidget;
extern std::vector<SP<CHyprspaceWidget>> g_overviewWidgets;

// input / render hooks
extern SP<HOOK_CALLBACK_FN> g_pRenderHook;
extern SP<HOOK_CALLBACK_FN> g_pConfigReloadHook;
extern SP<HOOK_CALLBACK_FN> g_pMouseButtonHook;
extern SP<HOOK_CALLBACK_FN> g_pMouseMotionHook;
extern SP<HOOK_CALLBACK_FN> g_pMouseAxisHook;
extern SP<HOOK_CALLBACK_FN> g_pSwipeBeginHook;
extern SP<HOOK_CALLBACK_FN> g_pSwipeUpdateHook;
extern SP<HOOK_CALLBACK_FN> g_pSwipeEndHook;
extern SP<HOOK_CALLBACK_FN> g_pAddMonitorHook;

// IPC workspace event hooks
extern SP<HOOK_CALLBACK_FN> g_pWorkspaceHook;
extern SP<HOOK_CALLBACK_FN> g_pFocusedMonHook;
extern SP<HOOK_CALLBACK_FN> g_pCreateWorkspaceHook;
extern SP<HOOK_CALLBACK_FN> g_pDestroyWorkspaceHook;
extern SP<HOOK_CALLBACK_FN> g_pMoveWorkspaceHook;
extern SP<HOOK_CALLBACK_FN> g_pOpenWindowHook;
extern SP<HOOK_CALLBACK_FN> g_pCloseWindowHook;
extern SP<HOOK_CALLBACK_FN> g_pUrgentHook;

namespace Config {
    extern int   panelHeight;
    extern int   workspaceMargin;
    extern bool  onBottom;
    extern bool  affectStrut;
    extern bool  disableBlur;
    extern bool  autoDrag;
    extern bool  autoScroll;
    extern bool  exitOnClick;
    extern bool  exitOnSwitch;
    extern bool  showNewWorkspace;
    extern bool  showEmptyWorkspace;
    extern bool  disableGestures;
    extern bool  reverseSwipe;
    extern float overrideAnimSpeed;
}

SP<CHyprspaceWidget> getWidgetForMonitor(PHLMONITOR mon);
void                 registerMonitors();
