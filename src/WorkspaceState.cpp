#include "WorkspaceState.hpp"
#include "Globals.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <algorithm>

void WorkspaceManager::markDirty(int id) {
    for (int d : m_dirty)
        if (d == id) return;
    m_dirty.push_back(id);
}

void WorkspaceManager::upsertWorkspace(int id, const std::string& name, int monitorID, bool special) {
    auto& ws     = m_workspaces[id];
    ws.id        = id;
    ws.name      = name;
    ws.monitorID = monitorID;
    ws.special   = special;
    ws.occupied  = ws.windowCount > 0;
    markDirty(id);
}

void WorkspaceManager::destroyWorkspace(int id) {
    m_workspaces.erase(id);
    markDirty(id);
    if (m_activeID == id) m_activeID = -1;
}

void WorkspaceManager::setActive(int id) {
    if (m_activeID == id) return;
    if (m_activeID != -1) {
        auto it = m_workspaces.find(m_activeID);
        if (it != m_workspaces.end()) {
            it->second.active = false;
            markDirty(m_activeID);
        }
    }
    m_activeID = id;
    auto it = m_workspaces.find(id);
    if (it != m_workspaces.end()) {
        it->second.active = true;
        markDirty(id);
    }
}

void WorkspaceManager::setUrgent(int id, bool urgent) {
    auto it = m_workspaces.find(id);
    if (it == m_workspaces.end()) return;
    if (it->second.urgent == urgent) return;
    it->second.urgent = urgent;
    markDirty(id);
}

void WorkspaceManager::refreshWindowCounts() {
    for (auto& [id, ws] : m_workspaces)
        ws.windowCount = 0;

    for (auto& win : g_pCompositor->m_windows) {
        if (!win || !win->m_workspace) continue;
        int wsID = win->m_workspace->m_id;
        auto it  = m_workspaces.find(wsID);
        if (it == m_workspaces.end()) continue;
        it->second.windowCount++;
        it->second.occupied = true;
        markDirty(wsID);
    }

    for (auto& [id, ws] : m_workspaces) {
        bool nowOccupied = ws.windowCount > 0;
        if (ws.occupied != nowOccupied) {
            ws.occupied = nowOccupied;
            markDirty(id);
        }
    }
}

void WorkspaceManager::syncFromCompositor() {
    m_workspaces.clear();
    m_dirty.clear();
    m_activeID = -1;

    for (auto& ws : g_pCompositor->getWorkspaces()) {
        if (!ws || ws->inert()) continue;
        int monID = ws->m_monitor ? static_cast<int>(ws->m_monitor->m_id) : -1;
        upsertWorkspace(ws->m_id, ws->m_name, monID, ws->m_isSpecialWorkspace);
    }

    for (auto& mon : g_pCompositor->m_monitors) {
        if (!mon || !mon->m_activeWorkspace) continue;
        setActive(mon->m_activeWorkspace->m_id);
        break;
    }

    refreshWindowCounts();
}

void WorkspaceManager::onIPCEvent(const std::string& event, const std::string& data) {
    if (event == "workspace") {
        for (auto& [id, ws] : m_workspaces) {
            if (ws.name == data) { setActive(id); return; }
        }
        return;
    }

    if (event == "focusedmon") {
        auto comma = data.find(',');
        if (comma == std::string::npos) return;
        std::string wsName = data.substr(comma + 1);
        for (auto& [id, ws] : m_workspaces) {
            if (ws.name == wsName) { setActive(id); return; }
        }
        return;
    }

    if (event == "createworkspace") {
        syncFromCompositor();
        return;
    }

    if (event == "destroyworkspace") {
        for (auto& [id, ws] : m_workspaces) {
            if (ws.name == data) { destroyWorkspace(id); return; }
        }
        return;
    }

    if (event == "moveworkspace") {
        syncFromCompositor();
        return;
    }

    if (event == "openwindow" || event == "closewindow") {
        refreshWindowCounts();
        return;
    }

    if (event == "urgent") {
        for (auto& win : g_pCompositor->m_windows) {
            if (!win || !win->m_workspace) continue;
            char addr[32];
            snprintf(addr, sizeof(addr), "%lx", (unsigned long)(uintptr_t)win.get());
            if (data.find(addr) != std::string::npos) {
                setUrgent(win->m_workspace->m_id, true);
                return;
            }
        }
        return;
    }
}

void WorkspaceManager::switchWorkspace(int id) {
    auto mon = g_pCompositor->getMonitorFromCursor();
    if (!mon) return;
    mon->changeWorkspace(id);
    setActive(id);
}

const Workspace* WorkspaceManager::activeWorkspace() const {
    if (m_activeID == -1) return nullptr;
    auto it = m_workspaces.find(m_activeID);
    return it != m_workspaces.end() ? &it->second : nullptr;
}

std::vector<Workspace> WorkspaceManager::snapshot() const {
    std::vector<Workspace> out;
    out.reserve(m_workspaces.size());
    for (auto& [id, ws] : m_workspaces)
        out.push_back(ws);
    std::sort(out.begin(), out.end(), [](const Workspace& a, const Workspace& b) {
        return a.id < b.id;
    });
    return out;
}
