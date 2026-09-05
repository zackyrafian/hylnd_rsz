#include "WorkspaceRenderer.hpp"
#include "Overview.hpp"
#include "Globals.hpp"

#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/render/pass/SurfacePassElement.hpp>
#include <hyprland/src/render/pass/RendererHintsPassElement.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <algorithm>
#include <climits>

static void renderRect(CBox box, CHyprColor color, bool blur = false) {
    CRectPassElement::SRectData d;
    d.box   = box;
    d.color = color;
    d.blur  = blur;
    d.blurA = blur ? 0.85f : 1.f;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(d));
}

static void renderWindowStub(PHLWINDOW pWindow, PHLMONITOR pMonitor,
                              PHLWORKSPACE pWsOverride,
                              CBox rectOverride, CBox clipBox,
                              const Time::steady_tp& time) {
    if (!pWindow || !pMonitor || !pWsOverride) return;
    if (!pWindow->m_isMapped || !pWindow->wlSurface() || !pWindow->wlSurface()->resource()) return;

    const auto oRealPosition = pWindow->m_realPosition->value();
    const auto oSize         = pWindow->m_realSize->value();
    const float logicalW     = std::max((float)oSize.x, 5.f);
    const float scaleMod     = rectOverride.w / std::max(logicalW * pMonitor->m_scale, 5.f);
    if (!(scaleMod > 0.f) || !(rectOverride.w > 0 && rectOverride.h > 0)) return;

    const Vector2D logicalTL = oRealPosition + pWindow->m_floatingOffset;
    const Vector2D scaledTL  = (logicalTL - pMonitor->m_position) * pMonitor->m_scale;
    const Vector2D translate = rectOverride.pos() / scaleMod - scaledTL;

    SRenderModifData renderModif;
    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, std::any(translate)});
    renderModif.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE,     std::any(scaleMod)});
    renderModif.enabled = true;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(
        CRendererHintsPassElement::SData{.renderModif = renderModif}));
    Hyprutils::Utils::CScopeGuard guard([] {
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(
            CRendererHintsPassElement::SData{.renderModif = SRenderModifData{}}));
    });

    g_pHyprRenderer->damageWindow(pWindow);

    CSurfacePassElement::SRenderData renderdata = {pMonitor, time};
    renderdata.pos               = oRealPosition + pWindow->m_floatingOffset;
    renderdata.w                 = std::max(oSize.x, 5.0);
    renderdata.h                 = std::max(oSize.y, 5.0);
    renderdata.surface           = pWindow->wlSurface()->resource();
    renderdata.dontRound         = pWindow->isEffectiveInternalFSMode(FSMODE_FULLSCREEN);
    renderdata.fadeAlpha         = 1.f;
    renderdata.alpha             = 1.f;
    renderdata.decorate          = false;
    renderdata.rounding          = renderdata.dontRound ? 0 : pWindow->rounding() * scaleMod * pMonitor->m_scale;
    renderdata.roundingPower     = renderdata.dontRound ? 2.f : pWindow->roundingPower();
    renderdata.blur              = false;
    renderdata.pWindow           = pWindow;
    renderdata.clipBox           = clipBox;
    renderdata.useNearestNeighbor = false;
    renderdata.squishOversized   = true;
    renderdata.surfaceCounter    = 0;

    pWindow->wlSurface()->resource()->breadthfirst(
        [&renderdata, &pWindow](SP<CWLSurfaceResource> s, const Vector2D& offset, void*) {
            if (!s || !s->m_current.texture) return;
            if (s->m_current.size.x < 1 || s->m_current.size.y < 1) return;
            renderdata.localPos    = offset;
            renderdata.texture     = s->m_current.texture;
            renderdata.surface     = s;
            renderdata.mainSurface = s == pWindow->wlSurface()->resource();
            g_pHyprRenderer->m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));
            renderdata.surfaceCounter++;
        },
        nullptr);
}

WorkspaceRenderer::WorkspaceRenderer(CHyprspaceWidget* owner) : m_owner(owner) {}

void WorkspaceRenderer::drawBackground(double panelX, double panelY,
                                       double panelW, double panelH) {
    renderRect(CBox{panelX, panelY, panelW, panelH},
               CHyprColor(0xCC1e1e2eU), !Config::disableBlur);
}

void WorkspaceRenderer::drawWorkspaceCell(const Workspace& ws,
                                          double x, double y,
                                          double w, double h,
                                          bool highlighted) {
    CHyprColor cellBg = ws.urgent   ? CHyprColor(0xCCf38ba8U)
                      : highlighted ? CHyprColor(0xCC89b4faU)
                      : ws.occupied ? CHyprColor(0xCC313244U)
                                    : CHyprColor(0xCC1e1e2eU);
    renderRect(CBox{x, y, w, h}, cellBg);
}

void WorkspaceRenderer::drawNewWorkspaceSlot(double x, double y,
                                              double w, double h) {
    renderRect(CBox{x, y, w, h}, CHyprColor(0x44a6e3a1U));
}

void WorkspaceRenderer::draw(const std::vector<Workspace>& workspaces, const std::vector<int>& dirtyIDs) {
    auto owner = m_owner->getOwner();
    if (!owner) return;

    const float curYOff = m_owner->m_curYOffset->value();
    const float panelH  = static_cast<float>(Config::panelHeight) * owner->m_scale;

    if (!m_owner->isActive() && !m_owner->m_curYOffset->isBeingAnimated()) return;

    const double monW = owner->m_transformedSize.x;
    const double monH = owner->m_transformedSize.y;

    CBox widgetBox = {
        0.0,
        Config::onBottom
            ? (monH - panelH) + curYOff
            : -curYOff,
        monW,
        panelH
    };

    const CBox monitorClip = {{0, 0}, owner->m_transformedSize};
    g_pHyprOpenGL->m_renderData.clipBox = monitorClip;

    g_pHyprRenderer->damageMonitor(owner);
    renderRect(widgetBox, CHyprColor(0xCC1e1e2eU), !Config::disableBlur);

    if (workspaces.empty()) {
        g_pHyprOpenGL->m_renderData.clipBox = monitorClip;
        return;
    }

    const double monitorSF = ((Config::panelHeight - 2.0 * Config::workspaceMargin)
                               / (monH / owner->m_scale))
                             * owner->m_scale;

    const double wsBoxW = monW * monitorSF;
    const double wsBoxH = (monH / owner->m_scale) * monitorSF * owner->m_scale;

    if (!(wsBoxW > 0 && wsBoxH > 0)) {
        g_pHyprOpenGL->m_renderData.clipBox = monitorClip;
        return;
    }

    const double groupW  = wsBoxW * workspaces.size()
                           + (Config::workspaceMargin * owner->m_scale) * (workspaces.size() - 1);

    double curX = (monW / 2.0) - (groupW / 2.0);
    double curY = Config::onBottom
        ? (monH - (Config::workspaceMargin * owner->m_scale) - wsBoxH) + curYOff
        : (Config::workspaceMargin * owner->m_scale) - curYOff;

    const auto time = Time::steadyNow();

    m_owner->clearWorkspaceBoxes();
    m_owner->clearWindowBoxes();

    for (const auto& ws : workspaces) {
        CBox wsBox = {curX, curY, wsBoxW, wsBoxH};

        bool isActive = ws.active;
        CHyprColor bgColor = ws.urgent    ? CHyprColor(0x99f38ba8U)
                           : isActive     ? CHyprColor(0x9989b4faU)
                           : ws.occupied  ? CHyprColor(0x99313244U)
                                          : CHyprColor(0x991e1e2eU);
        renderRect(wsBox, bgColor);

        auto pWs = g_pCompositor->getWorkspaceByID(ws.id);
        if (pWs) {
            for (auto& win : g_pCompositor->m_windows) {
                if (!win || !win->m_isMapped) continue;
                if (win->m_workspace != pWs) continue;
                if (win->m_isFloating && pWs->getLastFocusedWindow() == win) continue;

                double wX = curX + ((win->m_realPosition->value().x - owner->m_position.x)
                                    * monitorSF * owner->m_scale);
                double wY = curY + ((win->m_realPosition->value().y - owner->m_position.y)
                                    * monitorSF * owner->m_scale);
                double wW = win->m_realSize->value().x * monitorSF * owner->m_scale;
                double wH = win->m_realSize->value().y * monitorSF * owner->m_scale;
                if (!(wW > 0 && wH > 0)) continue;

                CBox winBox = {wX, wY, wW, wH};
                renderWindowStub(win, owner, pWs, winBox, wsBox, time);
                m_owner->addWindowBox(win, winBox);
            }

            auto lastFocused = pWs->getLastFocusedWindow();
            if (lastFocused && lastFocused->m_isFloating && lastFocused->m_isMapped) {
                double wX = curX + ((lastFocused->m_realPosition->value().x - owner->m_position.x)
                                    * monitorSF * owner->m_scale);
                double wY = curY + ((lastFocused->m_realPosition->value().y - owner->m_position.y)
                                    * monitorSF * owner->m_scale);
                double wW = lastFocused->m_realSize->value().x * monitorSF * owner->m_scale;
                double wH = lastFocused->m_realSize->value().y * monitorSF * owner->m_scale;
                if (wW > 0 && wH > 0) {
                    CBox winBox = {wX, wY, wW, wH};
                    renderWindowStub(lastFocused, owner, pWs, winBox, wsBox, time);
                    m_owner->addWindowBox(lastFocused, winBox);
                }
            }
        }

        if (isActive) {
            const double bord = 2 * owner->m_scale;
            renderRect(CBox{curX - bord, curY - bord, wsBoxW + bord*2, wsBoxH + bord*2}, CHyprColor(0xFF89b4faU));
            renderRect(wsBox, bgColor);
        }

        CBox inputBox = wsBox;
        inputBox.scale(1.0 / owner->m_scale);
        inputBox.x += owner->m_position.x;
        inputBox.y += owner->m_position.y;
        m_owner->addWorkspaceBox(ws.id, inputBox);

        curX += wsBoxW + Config::workspaceMargin * owner->m_scale;
    }

    if (Config::showNewWorkspace) {
        CBox newBox = {curX, curY, wsBoxW, wsBoxH};
        drawNewWorkspaceSlot(curX, curY, wsBoxW, wsBoxH);

        CBox inputBox = newBox;
        inputBox.scale(1.0 / owner->m_scale);
        inputBox.x += owner->m_position.x;
        inputBox.y += owner->m_position.y;
        m_owner->setNewWsBox(inputBox);
    }

    const auto& drag = m_owner->getDragState();
    if (drag.active && drag.window && drag.window->m_isMapped) {
        auto pWs = owner->m_activeWorkspace;
        if (pWs) {
            const double ghostW = wsBoxW * 0.3;
            const double ghostH = wsBoxH * 0.3;
            const double ghostX = (drag.currentPos.x - owner->m_position.x) * owner->m_scale - ghostW / 2.0;
            const double ghostY = (drag.currentPos.y - owner->m_position.y) * owner->m_scale - ghostH / 2.0;
            CBox ghostBox{ghostX, ghostY, ghostW, ghostH};
            renderWindowStub(drag.window, owner, pWs, ghostBox,
                             CBox{{0,0}, owner->m_transformedSize}, time);
        }
    }

    g_pHyprOpenGL->m_renderData.clipBox = monitorClip;
}
