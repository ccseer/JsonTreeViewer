#pragma once

#include <QByteArray>
#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace jtv {
namespace ui {

// --- Theme Colors (Seer Baseline) ---

namespace Colors {
// Shared
inline const QString Accent = "#0288D1";

// Dark Theme
inline const QString DarkBG      = "#121212";
inline const QString DarkSurface = "#1E1E1E";
inline const QString DarkText    = "#EEEEEE";
inline const QString DarkTextDim = "#9E9E9E";
inline const QString DarkBorder  = "#333333";
inline const QString DarkInput   = "#121212";

// Light Theme
inline const QString LightBG      = "#FFFFFF";
inline const QString LightSurface = "#F5F5F5";
inline const QString LightText    = "#212121";
inline const QString LightTextDim = "#757575";
inline const QString LightBorder  = "#E0E0E0";
inline const QString LightInput   = "#FFFFFF";
}  // namespace Colors

// --- SVGs (Material Symbols / Custom) ---

// Material Symbol: "Search"
constexpr auto g_svg_search = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24">
  <path fill="currentColor" d="M784-120 532-372q-30 24-69 38t-83 14q-109 0-184.5-75.5T120-580q0-109 75.5-184.5T380-840q109 0 184.5 75.5T640-580q0 44-14 83t-38 69l252 252-56 56ZM380-400q75 0 127.5-52.5T560-580q0-75-52.5-127.5T380-760q-75 0-127.5 52.5T200-580q0 75 52.5 127.5T380-400Z"/>
</svg>)SVG";

// Material Symbol: "Filter List"
constexpr auto g_svg_filter = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24">
  <path fill="currentColor" d="M440-160v-320L160-760v-80h640v80L520-480v320h-80Z"/>
</svg>)SVG";

// Material Symbol: "Public" (Globe)
constexpr auto g_svg_globe = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24">
  <path fill="currentColor" d="M480-80q-83 0-156-31.5T197-197q-54-54-85.5-127T80-480q0-83 31.5-156T197-763q54-54 127-85.5T480-880q83 0 156 31.5T763-763q54 54 85.5 127T880-480q0 83-31.5 156T763-197q-54 54-127 85.5T480-80Zm-40-82v-78q-33 0-56.5-23.5T360-320v-40L168-552q-3 18-5.5 36t-2.5 36q0 121 76.5 212T440-162Zm282-158q30-33 49-73t19-87q0-72-32.5-132.5T774-710L640-576v136h-80v-160L416-744v-56q16-2 32-3t32-1q120 0 219 73t137 185l-101-101q-5 10-12.5 18.5T706-613L560-467v75l162 162Z"/>
</svg>)SVG";

// Material Symbol: "Article" Rounded, Outline, Weight 300
constexpr auto g_svg_article = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24">
  <path fill="currentColor" d="M312-300h336v-44H312v44Zm0-160h336v-44H312v44Zm0-160h336v-44H312v44ZM228-156q-29.7 0-50.85-21.15Q156-198.3 156-228v-504q0-29.7 21.15-50.85Q198.3-804 228-804h504q29.7 0 50.85 21.15Q804-761.7 804-732v504q0 29.7-21.15 50.85Q761.7-156 732-156H228Zm0-72h504v-504H228v504Zm0 0v-504 504Z"/>
</svg>)SVG";

// Material Symbol: "Info" Rounded, Outline
constexpr auto g_svg_info = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24">
  <path fill="currentColor" d="M480-120q-75 0-140.5-28.5t-114-77q-48.5-48.5-77-114T120-480q0-75 28.5-140.5t77-114q48.5-48.5 114-77T480-840q75 0 140.5 28.5t114 77q48.5 48.5 77 114T840-480q0 75-28.5 140.5t-77 114q-48.5 48.5-114 77T480-120Zm0-72q120 0 204-84t84-204q0-120-84-204t-204-84q-120 0-204 84t-84 204q0 120 84 204t204 84Zm-40-101h80v-240h-80v240Zm40-327q17 0 28.5-11.5T520-660q0-17-11.5-28.5T480-700q-17 0-28.5 11.5T440-660q0 17 11.5 28.5T480-620Z"/>
</svg>)SVG";

// Material Symbol: "Chevron Right"
constexpr auto g_svg_chevron_right = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24">
  <path fill="currentColor" d="m376-300-44-44 136-136-136-136 44-44 180 180-180 180Z"/>
</svg>)SVG";

// Material Symbol: "Close"
constexpr auto g_svg_close = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24">
  <path fill="currentColor" d="m256-200-56-56 224-224-224-224 56-56 224 224 224-224 56 56-224 224 224 224-56 56-224-224-224 224Z"/>
</svg>)SVG";

// Material Symbol: "Curly Brackets" - Stroke version for better visibility
constexpr auto g_svg_object = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
  <path fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" d="M10 4H9a2 2 0 0 0-2 2v5a2 2 0 0 1-2 2a2 2 0 0 1 2 2v5a2 2 0 0 0 2 2h1m4-18h1a2 2 0 0 1 2 2v5a2 2 0 0 0 2 2a2 2 0 0 0-2 2v5a2 2 0 0 1-2 2h-1"/>
</svg>)SVG";

// Material Symbol: "Square Brackets" - Stroke version for better visibility
constexpr auto g_svg_array = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
  <path fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" d="M8 5H7a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2h1m8-14h1a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2h-1"/>
</svg>)SVG";

// --- QSS (Qt Style Sheets) ---

// Placeholder args: %1: SurfaceBG, %2: Border, %3: InputBG, %4: Text, %5:
// Accent
constexpr auto g_qss_top_bar = R"(
    QWidget#topBar { background-color: %1; border-bottom: 1px solid %2; }
    QLineEdit { 
        background-color: %3; border: 1px solid %2; border-radius: 4px; 
        color: %4; padding: 4px 10px; selection-background-color: %5; 
    }
    QLineEdit:focus { border: 1px solid %5; }
    QComboBox { 
        background-color: transparent; border: none; color: %4; font-size: 11px; 
    }
)";

constexpr auto g_qss_progress_bar = R"(
    QProgressBar { border: none; background: transparent; } 
    QProgressBar::chunk { background-color: %1; }
)";

constexpr auto g_qss_breadcrumb_btn = R"(
    QPushButton { border: none; padding: 2px 4px; color: %1; border-radius: 4px; } 
    QPushButton:hover { background-color: %2; }
)";

// Placeholder args: %1: stop0, %2: stop1, %3: Border, %4: LabelText, %5:
// ProgressBG, %6: ProgressChunk, %7: BtnText, %8: BtnHover
constexpr auto g_qss_search_banner = R"(
    QWidget#searchBanner { 
        background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2);
        border-bottom: 1px solid %3; 
    }
    QLabel { color: %4; font-weight: bold; }
    QProgressBar { 
        border: none; background: %5; height: 4px; border-radius: 2px;
    }
    QProgressBar::chunk { background-color: %6; border-radius: 2px; }
    QPushButton { color: %7; font-size: 16px; font-weight: bold; border: none; background: transparent; }
    QPushButton:hover { color: %8; }
)";

constexpr auto g_qss_search_list
    = "QListView { background-color: %1; border: none; }";

constexpr auto g_qss_label_query = "font-size: 12px; color: %1;";
constexpr auto g_qss_label_count = "font-size: 11px; color: %1;";
constexpr auto g_qss_value_label = "color: %1;";

// --- Helpers ---

inline QIcon svgIcon(const char* svg_data,
                     const QColor& color,
                     int icon_sz,
                     qreal dpr)
{
    QByteArray data(svg_data);
    QByteArray colorName = color.name(QColor::HexRgb).toUtf8();
    data.replace("currentColor", colorName);

    QSvgRenderer renderer(data);
    if (!renderer.isValid()) {
        return {};
    }

    const int phys = qRound(icon_sz * dpr);
    QPixmap pix(phys, phys);
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    renderer.render(&p, QRectF(0, 0, icon_sz, icon_sz));
    return QIcon(pix);
}

}  // namespace ui
}  // namespace jtv
