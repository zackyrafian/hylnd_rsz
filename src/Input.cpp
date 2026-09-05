#include "Overview.hpp"
#include "Globals.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>

bool CHyprspaceWidget::buttonEvent(bool pressed, Vector2D coords) {
    if (!m_active) return false;

    auto owner = getOwner();
    if (!owner) return false;

    if (pressed) {
        for (auto& [win, box] : m_windowBoxes) {
            if (!win) continue;
            if (box.containsPoint(coords * owner->m_scale)) {
                m_drag.active      = true;
                m_drag.window      = win;
                m_drag.startPos    = coords;
                m_drag.currentPos  = coords;
                m_drag.sourceWsID  = win->m_workspace ? win->m_workspace->m_id : -1;
                m_drag.windowBox   = box;
                return true;
            }
        }
        return false;
    }

    if (m_drag.active) {
        bool dropped = false;

        for (auto& [wsID, box] : m_workspaceBoxes) {
            if (box.containsPoint(coords * owner->m_scale)) {
                if (m_drag.window && m_drag.sourceWsID != wsID) {
                    auto ws = g_pCompositor->getWorkspaceByID(wsID);
                    if (ws) {
                        g_pCompositor->moveWindowToWorkspaceSafe(m_drag.window, ws);
                    }
                }
                dropped = true;
                break;
            }
        }

        if (!dropped && Config::showNewWorkspace && m_newWsBox.containsPoint(coords * owner->m_scale)) {
            int newID = 1;
            while (g_pCompositor->getWorkspaceByID(newID)) newID++;
            auto ws = g_pCompositor->createNewWorkspace(newID, m_ownerID);
            if (ws && m_drag.window)
                g_pCompositor->moveWindowToWorkspaceSafe(m_drag.window, ws);
            dropped = true;
        }

        m_drag = SDragState{};
        g_pCompositor->scheduleFrameForMonitor(owner);
        return true;
    }

    for (auto& [wsID, box] : m_workspaceBoxes) {
        if (box.containsPoint(coords * owner->m_scale)) {
            if (Config::exitOnClick) hide();
            owner->changeWorkspace(wsID);
            return true;
        }
    }

    if (Config::showNewWorkspace && m_newWsBox.containsPoint(coords * owner->m_scale)) {
        int newID = 1;
        while (g_pCompositor->getWorkspaceByID(newID)) newID++;
        (void)g_pCompositor->createNewWorkspace(newID, m_ownerID);
        owner->changeWorkspace(newID);
        if (Config::exitOnClick) hide();
        return true;
    }

    if (Config::exitOnClick) {
        hide();
        return true;
    }

    return false;
}

bool CHyprspaceWidget::motionEvent(Vector2D coords) {
    if (!m_active || !m_drag.active) return false;

    auto owner = getOwner();
    if (!owner) return false;

    m_drag.currentPos = coords;
    g_pCompositor->scheduleFrameForMonitor(owner);
    return true;
}

bool CHyprspaceWidget::axisEvent(double delta, wl_pointer_axis axis, Vector2D coords) {
    if (!m_active) return false;

    auto owner = getOwner();
    if (!owner) return false;

    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        if (delta < 0) {
            SWorkspaceIDName wsIDName = getWorkspaceIDNameFromString("r-1");
            if (!g_pCompositor->getWorkspaceByID(wsIDName.id))
                (void)g_pCompositor->createNewWorkspace(wsIDName.id, m_ownerID);
            owner->changeWorkspace(wsIDName.id);
        } else {
            SWorkspaceIDName wsIDName = getWorkspaceIDNameFromString("r+1");
            if (!g_pCompositor->getWorkspaceByID(wsIDName.id))
                (void)g_pCompositor->createNewWorkspace(wsIDName.id, m_ownerID);
            owner->changeWorkspace(wsIDName.id);
        }
        if (Config::exitOnSwitch) hide();
        return true;
    }

    return false;
}

bool CHyprspaceWidget::isSwiping() {
    return m_swiping;
}

bool CHyprspaceWidget::beginSwipe(IPointer::SSwipeBeginEvent ev) {
    if (ev.fingers != 3) return false;
    m_swiping           = true;
    m_activeBeforeSwipe = m_active;
    m_avgSwipeSpeed     = 0.0;
    m_swipePoints       = 0;
    return true;
}

bool CHyprspaceWidget::updateSwipe(IPointer::SSwipeUpdateEvent ev) {
    if (!m_swiping) return false;

    auto owner = getOwner();
    if (!owner) return false;

    const double delta = Config::reverseSwipe ? -ev.delta.y : ev.delta.y;

    m_avgSwipeSpeed = (m_avgSwipeSpeed * m_swipePoints + std::abs(delta)) / (m_swipePoints + 1);
    m_swipePoints++;

    const double panelH = Config::panelHeight * owner->m_scale;

    if (!Config::onBottom) {
        m_curSwipeOffset += delta;
        m_curSwipeOffset = std::clamp(m_curSwipeOffset, 0.0, panelH);
    } else {
        m_curSwipeOffset -= delta;
        m_curSwipeOffset = std::clamp(m_curSwipeOffset, 0.0, panelH);
    }
    m_curYOffset->setValueAndWarp(static_cast<float>(m_curSwipeOffset));

    g_pCompositor->scheduleFrameForMonitor(owner);
    return true;
}

bool CHyprspaceWidget::endSwipe(IPointer::SSwipeEndEvent ev) {
    if (!m_swiping) return false;
    m_swiping = false;

    auto owner = getOwner();
    if (!owner) return false;

    const double threshold = Config::panelHeight * owner->m_scale * 0.3;

    if (m_curSwipeOffset > threshold)
        show();
    else
        hide();

    return true;
}
