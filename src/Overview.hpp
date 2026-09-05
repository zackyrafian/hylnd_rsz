#pragma once

#include "WorkspaceState.hpp"
#include <hyprland/src/Compositor.hpp>
#include <hyprutils/animation/AnimationConfig.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <wayland-server-protocol.h>
#include <memory>

struct SDragState {
    bool      active       = false;
    PHLWINDOW window       = nullptr;
    Vector2D  startPos     = {};
    Vector2D  currentPos   = {};
    int       sourceWsID   = -1;
    CBox      windowBox    = {};
};

class WorkspaceRenderer;

class CHyprspaceWidget {
    bool      m_active    = false;
    MONITORID m_ownerID   = MONITOR_INVALID;

    Hyprutils::Animation::SAnimationPropertyConfig m_animCfg;
    Hyprutils::Animation::SAnimationPropertyConfig m_anim;

    std::vector<std::tuple<int, CBox>> m_workspaceBoxes;
    CBox m_newWsBox = {};
    std::vector<std::tuple<PHLWINDOW, CBox>> m_windowBoxes;

    std::vector<std::tuple<PHLWINDOWREF, eFullscreenMode>> m_prevFullscreen;
    std::vector<std::tuple<PHLLSREF, float>>               m_oLayerAlpha;

    bool   m_swiping           = false;
    bool   m_activeBeforeSwipe = false;
    double m_avgSwipeSpeed     = 0.0;
    int    m_swipePoints       = 0;
    double m_curSwipeOffset    = 10.0;

    SDragState m_drag;

    PHLANIMVAR<float> m_workspaceScrollOffset;

    WorkspaceManager                     m_wsManager;
    std::unique_ptr<WorkspaceRenderer>   m_renderer;
    std::vector<int>                     m_pendingDirty;

public:
    PHLANIMVAR<float> m_curYOffset;

    CHyprspaceWidget(MONITORID ownerID);
    ~CHyprspaceWidget();

    PHLMONITOR getOwner();
    bool       isActive() const { return m_active; }

    void show();
    void hide();
    void updateConfig();
    void draw();
    void updateLayout();

    void onIPCEvent(const std::string& event, const std::string& data);

    bool buttonEvent(bool pressed, Vector2D coords);
    bool axisEvent(double delta, wl_pointer_axis axis, Vector2D coords);
    bool motionEvent(Vector2D coords);
    bool isSwiping();
    bool beginSwipe(IPointer::SSwipeBeginEvent ev);
    bool updateSwipe(IPointer::SSwipeUpdateEvent ev);
    bool endSwipe(IPointer::SSwipeEndEvent ev);

    const SDragState& getDragState() const { return m_drag; }

    void clearWorkspaceBoxes()                        { m_workspaceBoxes.clear(); }
    void addWorkspaceBox(int wsID, CBox box)           { m_workspaceBoxes.emplace_back(wsID, box); }
    void clearWindowBoxes()                           { m_windowBoxes.clear(); }
    void addWindowBox(PHLWINDOW win, CBox box)         { m_windowBoxes.emplace_back(win, box); }
    void setNewWsBox(CBox box)                        { m_newWsBox = box; }
    void clearDirty()                                 { m_wsManager.clearDirty(); }
};
