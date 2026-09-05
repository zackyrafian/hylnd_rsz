#include "Overview.hpp"
#include "WorkspaceRenderer.hpp"
#include "Globals.hpp"

#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/desktop/view/LayerSurface.hpp>

CHyprspaceWidget::CHyprspaceWidget(MONITORID ownerID) : m_ownerID(ownerID) {
    m_animCfg = *g_pConfigManager->getAnimationPropertyConfig("windows");
    m_anim    = *m_animCfg.pValues.lock();

    if (Config::overrideAnimSpeed > 0)
        m_anim.internalSpeed = Config::overrideAnimSpeed;

    g_pAnimationManager->createAnimation(0.F, m_curYOffset, m_animCfg.pValues.lock(), AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.F, m_workspaceScrollOffset, m_animCfg.pValues.lock(), AVARDAMAGE_ENTIRE);

    auto owner = getOwner();
    const float startY = owner
        ? static_cast<float>(Config::panelHeight * owner->m_scale)
        : static_cast<float>(Config::panelHeight);
    m_curYOffset->setValueAndWarp(startY);
    m_workspaceScrollOffset->setValueAndWarp(0);

    m_renderer = std::make_unique<WorkspaceRenderer>(this);
    m_wsManager.syncFromCompositor();
}

CHyprspaceWidget::~CHyprspaceWidget() = default;

PHLMONITOR CHyprspaceWidget::getOwner() {
    return g_pCompositor->getMonitorFromID(m_ownerID);
}

void CHyprspaceWidget::onIPCEvent(const std::string& event, const std::string& data) {
    m_wsManager.onIPCEvent(event, data);
    auto owner = getOwner();
    if (owner && m_active)
        g_pCompositor->scheduleFrameForMonitor(owner);
}

void CHyprspaceWidget::show() {
    auto owner = getOwner();
    if (!owner) return;

    if (m_prevFullscreen.empty()) {
        for (auto& ws : g_pCompositor->getWorkspaces()) {
            if (!ws || !ws->m_monitor || ws->m_monitor->m_id != m_ownerID) continue;
            const auto w = ws->getFullscreenWindow();
            if (w && ws->m_fullscreenMode != FSMODE_NONE) {
                if (ws->m_fullscreenMode == FSMODE_FULLSCREEN)
                    w->m_wantsInitialFullscreen = true;
                m_prevFullscreen.emplace_back(PHLWINDOWREF(w), ws->m_fullscreenMode);
                g_pCompositor->setWindowFullscreenState(
                    w, Desktop::View::SFullscreenState{.internal = FSMODE_NONE, .client = FSMODE_NONE});
            }
        }
    }

    m_active = true;

    if (!m_swiping) {
        *m_curYOffset    = 0.F;
        m_curSwipeOffset = 10.0;
    }

    m_wsManager.syncFromCompositor();

    for (const auto& ws : m_wsManager.snapshot())
        m_wsManager.onIPCEvent("workspace", ws.name);

    updateLayout();
    g_pCompositor->scheduleFrameForMonitor(owner);
}

void CHyprspaceWidget::hide() {
    auto owner = getOwner();
    if (!owner) return;

    for (auto& [wref, mode] : m_prevFullscreen) {
        auto w = wref.lock();
        if (w)
            g_pCompositor->setWindowFullscreenState(
                w, Desktop::View::SFullscreenState{.internal = mode, .client = mode});
    }
    m_prevFullscreen.clear();

    m_drag = SDragState{};

    m_active = false;

    if (!m_swiping) {
        *m_curYOffset    = static_cast<float>(Config::panelHeight * owner->m_scale);
        m_curSwipeOffset = -10.0;
    }

    updateLayout();
    g_pCompositor->scheduleFrameForMonitor(owner);
}

void CHyprspaceWidget::updateConfig() {
    m_animCfg = *g_pConfigManager->getAnimationPropertyConfig("windows");
    m_anim    = *m_animCfg.pValues.lock();

    if (Config::overrideAnimSpeed > 0)
        m_anim.internalSpeed = Config::overrideAnimSpeed;

    g_pAnimationManager->createAnimation(
        m_curYOffset->value(), m_curYOffset, m_animCfg.pValues.lock(), AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(
        m_workspaceScrollOffset->value(), m_workspaceScrollOffset, m_animCfg.pValues.lock(), AVARDAMAGE_ENTIRE);

    m_wsManager.syncFromCompositor();
}

void CHyprspaceWidget::draw() {
    if (!m_renderer) return;

    const std::vector<int> dirty = m_wsManager.dirtyIDs();
    const auto             snap  = m_wsManager.snapshot();

    m_renderer->draw(snap, dirty);

    m_wsManager.clearDirty();
}
