#pragma once

#include "WorkspaceState.hpp"
#include <vector>

class CHyprspaceWidget;

class WorkspaceRenderer {
public:
    explicit WorkspaceRenderer(CHyprspaceWidget* owner);

    void draw(const std::vector<Workspace>& workspaces, const std::vector<int>& dirtyIDs);

private:
    CHyprspaceWidget* m_owner = nullptr;

    void drawBackground(double panelX, double panelY,
                        double panelW, double panelH);

    void drawWorkspaceCell(const Workspace& ws,
                           double x, double y,
                           double w, double h,
                           bool   highlighted);

    void drawWindowThumbnail(double x, double y,
                             double w, double h,
                             float  alpha);

    void drawNewWorkspaceSlot(double x, double y,
                              double w, double h);
};
