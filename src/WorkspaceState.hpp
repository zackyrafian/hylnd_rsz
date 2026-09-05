#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

struct Workspace {
    int         id        = -1;
    std::string name;

    int  monitorID   = -1;
    int  windowCount = 0;

    bool active   = false;
    bool occupied = false;
    bool urgent   = false;
    bool special  = false;
};

class WorkspaceManager {
public:
    // Called from IPC hooks (main compositor thread).
    // Supported event names (matching Hyprland socket2 names):
    //   workspace, focusedmon, createworkspace, destroyworkspace,
    //   moveworkspace, openwindow, closewindow, urgent
    void onIPCEvent(const std::string& event, const std::string& data);

    // Switch the active workspace on the monitor that owns `id`.
    void switchWorkspace(int id);

    // Returns nullptr if no active workspace is tracked.
    const Workspace* activeWorkspace() const;

    // Snapshot of all workspaces ordered by ID — safe to read from
    // the render thread after draining the dirty set.
    std::vector<Workspace> snapshot() const;

    // IDs that changed since the last call to clearDirty().
    const std::vector<int>& dirtyIDs() const { return m_dirty; }
    void clearDirty() { m_dirty.clear(); }

    // Sync full state from Hyprland compositor (called on plugin load
    // and on configReload).
    void syncFromCompositor();

private:
    std::unordered_map<int, Workspace> m_workspaces;

    std::vector<int> m_dirty;
    int m_activeID = -1;

    void markDirty(int id);
    void upsertWorkspace(int id, const std::string& name, int monitorID, bool special = false);
    void destroyWorkspace(int id);
    void setActive(int id);
    void setUrgent(int id, bool urgent);
    void refreshWindowCounts();
};
