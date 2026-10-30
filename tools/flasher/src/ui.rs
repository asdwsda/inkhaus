//! Look of the window (docs/ui/installer.md, variant C): black on white, hairlines, square marks,
//! block progress bar. No colour; the dark theme swaps the tokens.

use eframe::egui::{self, Color32, CornerRadius, FontFamily, FontId, Pos2, Rect, RichText, Sense, Shape, Stroke, StrokeKind, Theme, Vec2};
use std::sync::Arc;

pub const COLUMN: f32 = 560.0;
pub const MARGIN: f32 = 28.0;
pub const BUTTON_H: f32 = 60.0;
pub const BLOCKS: usize = 24;

#[derive(Clone, Copy)]
pub struct Palette {
    pub bg: Color32,
    pub ink: Color32,
    pub secondary: Color32,
    pub hairline: Color32,
    pub disabled: Color32,
}

pub fn palette(theme: Theme) -> Palette {
    match theme {
        Theme::Light => Palette {
            bg: Color32::from_rgb(0xFF, 0xFF, 0xFF),
            ink: Color32::from_rgb(0x11, 0x11, 0x11),
            secondary: Color32::from_rgb(0x55, 0x55, 0x55),
            hairline: Color32::from_rgb(0xBB, 0xBB, 0xBB),
            disabled: Color32::from_rgb(0x8A, 0x8A, 0x8A),
        },
        Theme::Dark => Palette {
            bg: Color32::from_rgb(0x1A, 0x1A, 0x1A),
            ink: Color32::from_rgb(0xF2, 0xF2, 0xF2),
            secondary: Color32::from_rgb(0xB0, 0xB0, 0xB0),
            hairline: Color32::from_rgb(0x55, 0x55, 0x55),
            disabled: Color32::from_rgb(0x88, 0x88, 0x88),
        },
    }
}

pub fn bold_family() -> FontFamily {
    FontFamily::Name("bold".into())
}

/// Embed DejaVu Sans (regular + bold, Latin subset) like the device and the website.
pub fn install_fonts(ctx: &egui::Context) {
    let mut fonts = egui::FontDefinitions::default();
    fonts.font_data.insert("dejavu".into(), Arc::new(egui::FontData::from_static(include_bytes!("../assets/DejaVuSans.ttf"))));
    fonts.font_data.insert("dejavu-bold".into(), Arc::new(egui::FontData::from_static(include_bytes!("../assets/DejaVuSans-Bold.ttf"))));
    // Keep egui's own fonts behind ours as fallback for characters outside the subset.
    fonts.families.entry(FontFamily::Proportional).or_default().insert(0, "dejavu".into());
    let mut bold = vec!["dejavu-bold".to_string()];
    bold.extend(fonts.families.get(&FontFamily::Proportional).cloned().unwrap_or_default());
    fonts.families.insert(bold_family(), bold);
    ctx.set_fonts(fonts);
}

/// Visuals for both themes: flat, square, ink strokes.
pub fn install_style(ctx: &egui::Context) {
    for theme in [Theme::Light, Theme::Dark] {
        let p = palette(theme);
        let mut v = if theme == Theme::Dark { egui::Visuals::dark() } else { egui::Visuals::light() };
        v.panel_fill = p.bg;
        v.window_fill = p.bg;
        v.extreme_bg_color = p.bg;
        v.faint_bg_color = p.bg;
        v.override_text_color = Some(p.ink);
        v.window_corner_radius = CornerRadius::ZERO;
        v.menu_corner_radius = CornerRadius::ZERO;
        v.window_shadow = egui::Shadow::NONE;
        v.popup_shadow = egui::Shadow::NONE;
        v.window_stroke = Stroke::new(2.0, p.ink);
        v.hyperlink_color = p.ink;
        v.selection.bg_fill = p.ink;
        v.selection.stroke = Stroke::new(1.0, p.bg);
        let w = &mut v.widgets;
        w.noninteractive.bg_stroke = Stroke::new(1.0, p.hairline);
        w.noninteractive.fg_stroke = Stroke::new(1.0, p.ink);
        for s in [&mut w.inactive, &mut w.hovered, &mut w.active, &mut w.open] {
            s.corner_radius = CornerRadius::ZERO;
            s.bg_fill = p.bg;
            s.weak_bg_fill = p.bg;
            s.bg_stroke = Stroke::new(2.0, p.ink);
            s.fg_stroke = Stroke::new(2.0, p.ink);
        }
        w.noninteractive.corner_radius = CornerRadius::ZERO;
        w.hovered.weak_bg_fill = p.hairline;
        w.hovered.bg_fill = p.hairline;
        w.active.weak_bg_fill = p.hairline;
        w.open.weak_bg_fill = p.bg;
        ctx.set_visuals_of(theme, v);
    }
    ctx.all_styles_mut(|s| {
        s.spacing.item_spacing = Vec2::new(10.0, 8.0);
        s.spacing.interact_size.y = 44.0;
        s.spacing.button_padding = Vec2::new(12.0, 8.0);
        s.text_styles.insert(egui::TextStyle::Body, FontId::new(16.0, FontFamily::Proportional));
        s.text_styles.insert(egui::TextStyle::Button, FontId::new(16.0, FontFamily::Proportional));
        s.text_styles.insert(egui::TextStyle::Small, FontId::new(14.0, FontFamily::Proportional));
        s.text_styles.insert(egui::TextStyle::Monospace, FontId::new(14.0, FontFamily::Monospace));
    });
}

pub fn bold(text: impl Into<String>, size: f32) -> RichText {
    RichText::new(text).font(FontId::new(size, bold_family()))
}

pub fn secondary(text: impl Into<String>, size: f32, p: &Palette) -> RichText {
    RichText::new(text).size(size).color(p.secondary)
}

/// Number of filled blocks for a progress fraction.
pub fn filled_blocks(frac: f32, total: usize) -> usize {
    ((frac.clamp(0.0, 1.0) * total as f32).round() as usize).min(total)
}

/// Start block of the moving block shown while the progress is unknown.
pub fn moving_block(seconds: f64, total: usize) -> usize {
    ((seconds * 6.0) as usize) % total.max(1)
}

/// The Inkhaus logo (docs/ui/logo-inkhaus.svg, viewBox 100) painted with shapes.
pub fn logo(ui: &mut egui::Ui, size: f32, p: &Palette) {
    let (rect, _) = ui.allocate_exact_size(Vec2::splat(size), Sense::hover());
    let s = size / 100.0;
    let at = |x: f32, y: f32| Pos2::new(rect.min.x + x * s, rect.min.y + y * s);
    let ink = p.ink;
    let g = ui.painter();
    let house = [(74.0, 90.0), (88.0, 90.0), (88.0, 46.0), (50.0, 12.0), (12.0, 46.0), (12.0, 90.0), (26.0, 90.0)];
    g.add(Shape::line(house.iter().map(|&(x, y)| at(x, y)).collect(), Stroke::new(9.0 * s, ink)));
    for &(x, y) in &house {
        g.circle_filled(at(x, y), 4.5 * s, ink);
    }
    g.circle_filled(at(50.0, 41.0), 9.0 * s, ink);
    g.add(Shape::convex_polygon(vec![at(50.0, 28.27), at(56.36, 34.64), at(43.64, 34.64)], ink, Stroke::NONE));
    g.rect_filled(Rect::from_min_max(at(38.0, 55.0), at(62.0, 90.0)), CornerRadius::same((12.0 * s).round() as u8), ink);
    g.circle_filled(at(50.0, 67.0), 7.0 * s, p.bg);
}

#[derive(Clone, Copy, PartialEq)]
pub enum Mark {
    Filled,
    Outline,
    Crossed,
}

/// Square status mark (filled / outline / crossed).
pub fn mark(ui: &mut egui::Ui, kind: Mark, size: f32, p: &Palette) {
    let (rect, _) = ui.allocate_exact_size(Vec2::splat(size), Sense::hover());
    let g = ui.painter();
    match kind {
        Mark::Filled => {
            g.rect_filled(rect, CornerRadius::ZERO, p.ink);
        }
        Mark::Outline | Mark::Crossed => {
            let inner = rect.shrink(1.0);
            g.rect_stroke(inner, CornerRadius::ZERO, Stroke::new(2.0, p.ink), StrokeKind::Inside);
            if kind == Mark::Crossed {
                let i = inner.shrink(4.0);
                g.line_segment([i.left_top(), i.right_bottom()], Stroke::new(2.0, p.ink));
                g.line_segment([i.right_top(), i.left_bottom()], Stroke::new(2.0, p.ink));
            }
        }
    }
}

/// 3 px horizontal rule across the available width.
pub fn rule(ui: &mut egui::Ui, p: &Palette) {
    let (rect, _) = ui.allocate_exact_size(Vec2::new(ui.available_width(), 3.0), Sense::hover());
    ui.painter().rect_filled(rect, CornerRadius::ZERO, p.ink);
}

/// 24-block progress bar. `None` shows a moving block.
pub fn block_bar(ui: &mut egui::Ui, frac: Option<f32>, p: &Palette) {
    let h = 22.0;
    let (rect, _) = ui.allocate_exact_size(Vec2::new(ui.available_width(), h), Sense::hover());
    let gap = 3.0;
    let w = (rect.width() - gap * (BLOCKS as f32 - 1.0)) / BLOCKS as f32;
    let (filled, moving) = match frac {
        Some(f) => (filled_blocks(f, BLOCKS), None),
        None => (0, Some(moving_block(ui.input(|i| i.time), BLOCKS))),
    };
    for i in 0..BLOCKS {
        let x = rect.min.x + i as f32 * (w + gap);
        let r = Rect::from_min_size(Pos2::new(x, rect.min.y), Vec2::new(w, h));
        let on = i < filled || moving.is_some_and(|m| (0..3).any(|k| (m + k) % BLOCKS == i));
        if on {
            ui.painter().rect_filled(r, CornerRadius::ZERO, p.ink);
        } else {
            ui.painter().rect_stroke(r, CornerRadius::ZERO, Stroke::new(2.0, p.ink), StrokeKind::Inside);
        }
    }
}

#[derive(Clone, Copy, PartialEq)]
pub enum Kind {
    Primary,
    Secondary,
}

/// Square button, 3 px border; a disabled button is a dashed outline. Returns clicked.
pub fn button(ui: &mut egui::Ui, label: &str, kind: Kind, enabled: bool, size: Vec2, font: f32, p: &Palette) -> bool {
    let sense = if enabled { Sense::click() } else { Sense::hover() };
    let (rect, resp) = ui.allocate_exact_size(size, sense);
    let g = ui.painter();
    let (fill, text) = match (enabled, kind) {
        (false, _) => (None, p.disabled),
        (true, Kind::Primary) => (Some(p.ink), p.bg),
        (true, Kind::Secondary) => (None, p.ink),
    };
    let hot = enabled && (resp.hovered() || resp.is_pointer_button_down_on());
    match (enabled, fill) {
        (false, _) => {
            let r = rect.shrink(1.5);
            let st = Stroke::new(3.0, p.hairline);
            let pts = [r.left_top(), r.right_top(), r.right_bottom(), r.left_bottom(), r.left_top()];
            g.extend(Shape::dashed_line(&pts, st, 8.0, 5.0));
        }
        (true, Some(c)) => {
            g.rect_filled(rect, CornerRadius::ZERO, c);
            if hot {
                g.rect_stroke(rect.shrink(4.0), CornerRadius::ZERO, Stroke::new(1.5, p.bg), StrokeKind::Inside);
            }
        }
        (true, None) => {
            if hot {
                g.rect_filled(rect, CornerRadius::ZERO, p.hairline);
            }
            g.rect_stroke(rect, CornerRadius::ZERO, Stroke::new(3.0, p.ink), StrokeKind::Inside);
        }
    }
    g.text(rect.center(), egui::Align2::CENTER_CENTER, label, FontId::new(font, bold_family()), text);
    if resp.has_focus() {
        g.rect_stroke(rect.expand(5.0), CornerRadius::ZERO, Stroke::new(3.0, p.ink), StrokeKind::Inside);
    }
    enabled && resp.clicked()
}

/// Underlined small text that acts as a link or toggle.
pub fn link(ui: &mut egui::Ui, text: &str) -> egui::Response {
    ui.add(egui::Label::new(RichText::new(text).size(14.0).underline()).sense(Sense::click())).on_hover_cursor(egui::CursorIcon::PointingHand)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn blocks_follow_fraction() {
        assert_eq!(filled_blocks(0.0, 24), 0);
        assert_eq!(filled_blocks(0.42, 24), 10);
        assert_eq!(filled_blocks(1.0, 24), 24);
        assert_eq!(filled_blocks(7.0, 24), 24);
        assert_eq!(filled_blocks(-1.0, 24), 0);
    }

    #[test]
    fn moving_block_stays_in_range() {
        for t in [0.0, 1.0, 100.3, 1e6] {
            assert!(moving_block(t, 24) < 24);
        }
    }

    #[test]
    fn dark_swaps_tokens() {
        let (l, d) = (palette(Theme::Light), palette(Theme::Dark));
        assert_ne!(l.bg, d.bg);
        assert_eq!(l.ink, Color32::from_rgb(0x11, 0x11, 0x11));
        assert_eq!(d.bg, Color32::from_rgb(0x1A, 0x1A, 0x1A));
    }
}
