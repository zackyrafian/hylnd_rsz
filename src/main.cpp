#include "Globals.hpp"
#include "Overview.hpp"

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/devices/IPointer.hpp>

std::vector<SP<CHyprspaceWidget>> g_overviewWidgets;

SP<HOOK_CALLBACK_FN> g_pRenderHook;
SP<HOOK_CALLBACK_FN> g_pConfigReloadHook;
SP<HOOK_CALLBACK_FN> g_pMouseButtonHook;
SP<HOOK_CALLBACK_FN> g_pMouseMotionHook;
SP<HOOK_CALLBACK_FN> g_pMouseAxisHook;
SP<HOOK_CALLBACK_FN> g_pSwipeBeginHook;
SP<HOOK_CALLBACK_FN> g_pSwipeUpdateHook;
SP<HOOK_CALLBACK_FN> g_pSwipeEndHook;
SP<HOOK_CALLBACK_FN> g_pAddMonitorHook;

// IPC workspace event hooks
SP<HOOK_CALLBACK_FN> g_pWorkspaceHook;
SP<HOOK_CALLBACK_FN> g_pFocusedMonHook;
SP<HOOK_CALLBACK_FN> g_pCreateWorkspaceHook;
SP<HOOK_CALLBACK_FN> g_pDestroyWorkspaceHook;
SP<HOOK_CALLBACK_FN> g_pMoveWorkspaceHook;
SP<HOOK_CALLBACK_FN> g_pOpenWindowHook;
SP<HOOK_CALLBACK_FN> g_pCloseWindowHook;
SP<HOOK_CALLBACK_FN> g_pUrgentHook;

namespace Config {
    int   panelHeight        = 250;
    int   workspaceMargin    = 12;
    bool  onBottom           = true;
    bool  affectStrut        = true;
    bool  disableBlur        = false;
    bool  autoDrag           = true;
    bool  autoScroll         = true;
    bool  exitOnClick        = true;
    bool  exitOnSwitch       = false;
    bool  showNewWorkspace   = true;
    bool  showEmptyWorkspace = true;
    bool  disableGestures    = false;
    bool  reverseSwipe       = false;
    float overrideAnimSpeed  = 0.f;
}

SP<CHyprspaceWidget> getWidgetForMonitor(PHLMONITOR mon) {
    if (!mon) return nullptr;
    for (auto& w : g_overviewWidgets) {
        if (w && w->getOwner() == mon) return w;
    }
    return nullptr;
}

void registerMonitors() {
    for (auto& mon : g_pCompositor->m_monitors) {
        if (!mon) continue;
        bool found = false;
        for (auto& w : g_overviewWidgets) {
            if (w && w->getOwner() == mon) { found = true; break; }
        }
        if (!found)
            g_overviewWidgets.push_back(makeShared<CHyprspaceWidget>(mon->m_id));
    }
    // remove stale widgets
    g_overviewWidgets.erase(
        std::remove_if(g_overviewWidgets.begin(), g_overviewWidgets.end(),
            [](const SP<CHyprspaceWidget>& w) { return !w || !w->getOwner(); }),
        g_overviewWidgets.end());
}

static void reloadConfig() {
    for (auto& w : g_overviewWidgets) {
        if (w) w->updateConfig();
    }
}

static void broadcastIPCEvent(const std::string& event, const std::string& data) {
    for (auto& w : g_overviewWidgets) {
        if (w) w->onIPCEvent(event, data);
    }
}

static SDispatchResult dispatchToggle(std::string args) {
    auto mon = g_pCompositor->getMonitorFromCursor();
    auto w   = getWidgetForMonitor(mon);
    if (!w) return SDispatchResult{};
    w->isActive() ? w->hide() : w->show();
    return SDispatchResult{};
}

static SDispatchResult dispatchShow(std::string args) {
    auto mon = g_pCompositor->getMonitorFromCursor();
    auto w   = getWidgetForMonitor(mon);
    if (w) w->show();
    return SDispatchResult{};
}

static SDispatchResult dispatchHide(std::string args) {
    auto mon = g_pCompositor->getMonitorFromCursor();
    auto w   = getWidgetForMonitor(mon);
    if (w) w->hide();
    return SDispatchResult{};
}

APICALL EXPORT std::string pluginAPIVersion() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO pluginInit(HANDLE handle) {
    pHandle = handle;

    HyprlandAPI::addConfigValue(handle, "plugin:overview:panelHeight",       Hyprlang::CConfigValue((Hyprlang::INT)250));
    HyprlandAPI::addConfigValue(handle, "plugin:overview:workspaceMargin",   Hyprlang::CConfigValue((Hyprlang::INT)12));
    HyprlandAPI::addConfigValue(handle, "plugin:overview:onBottom",          Hyprlang::CConfigValue((Hyprlang::INT)1));
    HyprlandAPI::addConfigValue(handle, "plugin:overview:affectStrut",       Hyprlang::CConfigValue((Hyprlang::INT)1));
    HyprlandAPI::addConfigValue(handle, "plugin:overview:disableBlur",       Hyprlang::CConfigValue((Hyprlang::INT)0));
    HyprlandAPI::addConfigValue(handle, "plugin:overview:exitOnClick",       Hyprlang::CConfigValue((Hyprlang::INT)1));
    HyprlandAPI::addConfigValue(handle, "plugin:overview:exitOnSwitch",      Hyprlang::CConfigValue((Hyprlang::INT)0));
    HyprlandAPI::addConfigValue(handle, "plugin:overview:showNewWorkspace",  Hyprlang::CConfigValue((Hyprlang::INT)1));
    HyprlandAPI::addConfigValue(handle, "plugin:overview:showEmptyWorkspace",Hyprlang::CConfigValue((Hyprlang::INT)1));
    HyprlandAPI::addConfigValue(handle, "plugin:overview:disableGestures",   Hyprlang::CConfigValue((Hyprlang::INT)0));
    HyprlandAPI::addConfigValue(handle, "plugin:overview:reverseSwipe",      Hyprlang::CConfigValue((Hyprlang::INT)0));

    HyprlandAPI::addDispatcherV2(handle, "overview:toggle", dispatchToggle);
    HyprlandAPI::addDispatcherV2(handle, "overview:show",   dispatchShow);
    HyprlandAPI::addDispatcherV2(handle, "overview:hide",   dispatchHide);

    g_pRenderHook = g_pHookSystem->hookDynamic("render", [](void*, SCallbackInfo&, std::any data) {
        const auto stage = std::any_cast<eRenderStage>(data);
        if (stage != eRenderStage::RENDER_POST_WINDOWS) return;

        const auto mon = g_pHyprOpenGL->m_renderData.pMonitor.lock();
        if (!mon) return;

        const auto w = getWidgetForMonitor(mon);
        if (w) w->draw();
    });

    g_pConfigReloadHook = g_pHookSystem->hookDynamic("configReloaded", [](void*, SCallbackInfo&, std::any) {
        reloadConfig();
    });

    g_pWorkspaceHook = g_pHookSystem->hookDynamic("workspace", [](void*, SCallbackInfo&, std::any data) {
        auto ws = std::any_cast<PHLWORKSPACE>(data);
        if (!ws) return;
        broadcastIPCEvent("workspace", ws->m_name);
    });

    g_pFocusedMonHook = g_pHookSystem->hookDynamic("focusedmon", [](void*, SCallbackInfo&, std::any data) {
        auto mon = std::any_cast<PHLMONITOR>(data);
        if (!mon || !mon->m_activeWorkspace) return;
        broadcastIPCEvent("focusedmon", mon->m_name + "," + mon->m_activeWorkspace->m_name);
    });

    g_pCreateWorkspaceHook = g_pHookSystem->hookDynamic("createworkspace", [](void*, SCallbackInfo&, std::any data) {
        auto ws = std::any_cast<PHLWORKSPACE>(data);
        if (!ws) return;
        broadcastIPCEvent("createworkspace", ws->m_name);
    });

    g_pDestroyWorkspaceHook = g_pHookSystem->hookDynamic("destroyworkspace", [](void*, SCallbackInfo&, std::any data) {
        auto ws = std::any_cast<PHLWORKSPACE>(data);
        if (!ws) return;
        broadcastIPCEvent("destroyworkspace", ws->m_name);
    });

    g_pMoveWorkspaceHook = g_pHookSystem->hookDynamic("moveworkspace", [](void*, SCallbackInfo&, std::any data) {
        auto ws = std::any_cast<PHLWORKSPACE>(data);
        if (!ws) return;
        broadcastIPCEvent("moveworkspace", ws->m_name);
    });

    g_pOpenWindowHook = g_pHookSystem->hookDynamic("openwindow", [](void*, SCallbackInfo&, std::any data) {
        auto win = std::any_cast<PHLWINDOW>(data);
        if (!win || !win->m_workspace) return;
        broadcastIPCEvent("openwindow", win->m_workspace->m_name);
    });

    g_pCloseWindowHook = g_pHookSystem->hookDynamic("closewindow", [](void*, SCallbackInfo&, std::any data) {
        auto win = std::any_cast<PHLWINDOW>(data);
        if (!win || !win->m_workspace) return;
        broadcastIPCEvent("closewindow", win->m_workspace->m_name);
    });

    g_pUrgentHook = g_pHookSystem->hookDynamic("urgent", [](void*, SCallbackInfo&, std::any data) {
        auto win = std::any_cast<PHLWINDOW>(data);
        if (!win || !win->m_workspace) return;
        broadcastIPCEvent("urgent", win->m_workspace->m_name);
    });

    g_pMouseButtonHook = g_pHookSystem->hookDynamic("mouseButton", [](void*, SCallbackInfo& info, std::any data) {
        auto ev  = std::any_cast<IPointer::SButtonEvent>(data);
        auto mon = g_pCompositor->getMonitorFromCursor();
        auto w   = getWidgetForMonitor(mon);
        if (!w || !w->isActive()) return;

        const bool pressed = ev.state == WL_POINTER_BUTTON_STATE_PRESSED;
        const auto pos     = g_pInputManager->getMouseCoordsInternal();
        if (w->buttonEvent(pressed, pos))
            info.cancelled = true;
    });

    g_pMouseMotionHook = g_pHookSystem->hookDynamic("mouseMove", [](void*, SCallbackInfo&, std::any) {
        auto mon = g_pCompositor->getMonitorFromCursor();
        auto w   = getWidgetForMonitor(mon);
        if (!w || !w->isActive()) return;
        const auto pos = g_pInputManager->getMouseCoordsInternal();
        w->motionEvent(pos);
    });

    g_pMouseAxisHook = g_pHookSystem->hookDynamic("mouseAxis", [](void*, SCallbackInfo& info, std::any data) {
        auto ev  = std::any_cast<IPointer::SAxisEvent>(data);
        auto mon = g_pCompositor->getMonitorFromCursor();
        auto w   = getWidgetForMonitor(mon);
        if (!w || !w->isActive()) return;

        const auto pos = g_pInputManager->getMouseCoordsInternal();
        if (w->axisEvent(ev.delta, ev.axis, pos))
            info.cancelled = true;
    });

    if (!Config::disableGestures) {
        g_pSwipeBeginHook = g_pHookSystem->hookDynamic("swipeBegin", [](void*, SCallbackInfo&, std::any data) {
            auto ev  = std::any_cast<IPointer::SSwipeBeginEvent>(data);
            auto mon = g_pCompositor->getMonitorFromCursor();
            auto w   = getWidgetForMonitor(mon);
            if (w) w->beginSwipe(ev);
        });

        g_pSwipeUpdateHook = g_pHookSystem->hookDynamic("swipeUpdate", [](void*, SCallbackInfo&, std::any data) {
            auto ev  = std::any_cast<IPointer::SSwipeUpdateEvent>(data);
            auto mon = g_pCompositor->getMonitorFromCursor();
            auto w   = getWidgetForMonitor(mon);
            if (w) w->updateSwipe(ev);
        });

        g_pSwipeEndHook = g_pHookSystem->hookDynamic("swipeEnd", [](void*, SCallbackInfo&, std::any data) {
            auto ev  = std::any_cast<IPointer::SSwipeEndEvent>(data);
            auto mon = g_pCompositor->getMonitorFromCursor();
            auto w   = getWidgetForMonitor(mon);
            if (w) w->endSwipe(ev);
        });
    }

    g_pAddMonitorHook = g_pHookSystem->hookDynamic("monitorAdded", [](void*, SCallbackInfo&, std::any) {
        registerMonitors();
    });

    registerMonitors();

    HyprlandAPI::addNotification(handle, "[Hyprspace] Loaded OK", CHyprColor(0xFF89b4faU), 3000);

    return {"Hyprspace", "Workspace overview panel", "hbar", "0.1"};
}

APICALL EXPORT void pluginExit() {
    g_pRenderHook.reset();
    g_pConfigReloadHook.reset();
    g_pMouseButtonHook.reset();
    g_pMouseMotionHook.reset();
    g_pMouseAxisHook.reset();
    g_pSwipeBeginHook.reset();
    g_pSwipeUpdateHook.reset();
    g_pSwipeEndHook.reset();
    g_pAddMonitorHook.reset();
    g_pWorkspaceHook.reset();
    g_pFocusedMonHook.reset();
    g_pCreateWorkspaceHook.reset();
    g_pDestroyWorkspaceHook.reset();
    g_pMoveWorkspaceHook.reset();
    g_pOpenWindowHook.reset();
    g_pCloseWindowHook.reset();
    g_pUrgentHook.reset();
    g_overviewWidgets.clear();
    pHandle = nullptr;
}
