#include "Overview.hpp"
#include "Globals.hpp"

#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/desktop/reserved/ReservedArea.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>

void CHyprspaceWidget::updateLayout() {
    if (!Config::affectStrut) return;

    auto pMonitor = getOwner();
    if (!pMonitor) return;

    if (m_active) {
        if (!Config::onBottom)
            pMonitor->m_reservedArea = Desktop::CReservedArea(Config::panelHeight, 0, 0, 0);
        else
            pMonitor->m_reservedArea = Desktop::CReservedArea(0, 0, Config::panelHeight, 0);
    } else {
        pMonitor->m_reservedArea = Desktop::CReservedArea();
    }

    g_pHyprRenderer->arrangeLayersForMonitor(m_ownerID);
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m_ownerID);
}
