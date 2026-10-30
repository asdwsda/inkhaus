//! The window.

use crate::device::{self, PortEntry};
use crate::error::Error;
use crate::flash::{self, Phase};
use crate::provision;
use crate::releases::{self, Release};
use crate::source;
use crate::ui::{self, Kind, Mark, Palette};
use eframe::egui::{self, Vec2};
use std::path::PathBuf;
use std::sync::mpsc::{channel, Receiver, TryRecvError};
use std::time::{Duration, Instant};

enum Releases {
    Loading,
    Ready(Vec<Release>),
    Failed(Error),
}

enum Msg {
    Progress(String, Option<f32>),
    Done(Result<String, Error>),
    Uploaded(Result<Vec<String>, Error>),
}

enum Outcome {
    Ok(String),
    Err(Error),
}

pub struct App {
    releases: Releases,
    rx_releases: Receiver<Result<Vec<Release>, Error>>,
    ports: Vec<PortEntry>,
    last_scan: Option<Instant>,
    chosen_port: Option<String>,
    show_pre: bool,
    chosen_tag: Option<String>,
    local: Option<PathBuf>,
    job: Option<Receiver<Msg>>,
    progress: (String, Option<f32>),
    outcome: Option<Outcome>,
    upload_note: Option<Result<Vec<String>, Error>>,
    last_upload: bool,
    upload_names: Vec<String>,
    show_details: bool,
    #[cfg(debug_assertions)]
    preview_port: Option<String>,
}

impl App {
    pub fn new() -> Self {
        let app = Self::new_inner();
        #[cfg(debug_assertions)]
        let app = app.preview();
        app
    }

    fn new_inner() -> Self {
        let (tx, rx) = channel();
        let repo = std::env::var("FLASHER_REPO").unwrap_or_else(|_| releases::DEFAULT_REPO.into());
        std::thread::spawn(move || {
            let _ = tx.send(releases::fetch(&repo));
        });
        App {
            releases: Releases::Loading,
            rx_releases: rx,
            ports: Vec::new(),
            last_scan: None,
            chosen_port: None,
            show_pre: false,
            chosen_tag: None,
            local: None,
            job: None,
            progress: (String::new(), Some(0.0)),
            outcome: None,
            upload_note: None,
            last_upload: false,
            upload_names: Vec::new(),
            show_details: false,
            #[cfg(debug_assertions)]
            preview_port: None,
        }
    }

    /// The port to use: the only candidate, else the user's choice.
    fn port(&self) -> Option<String> {
        #[cfg(debug_assertions)]
        if self.preview_port.is_some() {
            return self.preview_port.clone();
        }
        let c = device::candidates(&self.ports);
        if c.len() == 1 {
            return Some(c[0].name.clone());
        }
        self.chosen_port.clone().filter(|p| self.ports.iter().any(|q| &q.name == p))
    }

    fn selected_release(&self) -> Option<Release> {
        let Releases::Ready(all) = &self.releases else { return None };
        let list = releases::visible(all, self.show_pre);
        let tag = self.chosen_tag.as_ref();
        list.iter()
            .find(|r| Some(&r.tag_name) == tag)
            .or_else(|| list.get(releases::default_index(&list)))
            .map(|r| (*r).clone())
    }

    fn start(&mut self, ctx: &egui::Context) {
        let Some(port) = self.port() else {
            self.outcome = Some(Outcome::Err(Error::NoDevice));
            return;
        };
        let release = if self.local.is_some() { None } else { self.selected_release() };
        let local = self.local.clone();
        if release.is_none() && local.is_none() {
            return;
        }
        let (tx, rx) = channel();
        self.job = Some(rx);
        self.outcome = None;
        self.upload_note = None;
        self.last_upload = false;
        self.show_details = false;
        self.progress = ("Downloading".into(), Some(0.0));
        let ctx = ctx.clone();
        std::thread::spawn(move || {
            let send = |m: Msg| {
                let _ = tx.send(m);
                ctx.request_repaint();
            };
            let result = (|| -> Result<String, Error> {
                let tmp = tempfile::tempdir()?;
                if let Some(p) = &local {
                    send(Msg::Progress("Preparing".into(), Some(0.0)));
                    source::import_local(p, tmp.path())?;
                } else if let Some(r) = &release {
                    source::fetch_release(r, tmp.path(), &mut |f| send(Msg::Progress("Downloading".into(), Some(f))))?;
                }
                send(Msg::Progress("Checking files".into(), Some(0.0)));
                let image = source::load_dir(tmp.path())?;
                flash::flash(&port, &image, &mut |ph: Phase, f| send(Msg::Progress(ph.text().into(), if ph == Phase::Connecting { None } else { Some(f) })))?;
                Ok(image.manifest.version)
            })();
            send(Msg::Done(result));
        });
    }

    fn start_upload(&mut self, ctx: &egui::Context) {
        let Some(port) = self.port() else {
            self.outcome = Some(Outcome::Err(Error::NoDevice));
            return;
        };
        let Some(paths) = rfd::FileDialog::new().set_title("Choose config.yaml and/or secrets.yaml").add_filter("YAML", &["yaml"]).pick_files() else { return };
        let mut files = Vec::new();
        for p in paths {
            let name = p.file_name().map(|n| n.to_string_lossy().to_string()).unwrap_or_default();
            if !provision::valid_name(&name) {
                self.upload_note = Some(Err(Error::Flash(format!("{name} cannot be uploaded; the files must be called config.yaml or secrets.yaml"))));
                return;
            }
            match std::fs::read(&p) {
                Ok(d) => files.push((name, d)),
                Err(e) => {
                    self.upload_note = Some(Err(Error::from(e)));
                    return;
                }
            }
        }
        let (tx, rx) = channel();
        self.job = Some(rx);
        self.outcome = None;
        self.upload_note = None;
        self.last_upload = true;
        self.show_details = false;
        self.upload_names = files.iter().map(|(n, _)| n.clone()).collect();
        self.progress = ("Connecting to the M5Paper".into(), None);
        let ctx = ctx.clone();
        std::thread::spawn(move || {
            let send = |m: Msg| {
                let _ = tx.send(m);
                ctx.request_repaint();
            };
            let r = provision::upload_files(&port, &files, &mut |n, f| send(Msg::Progress(if f.is_none() { "Connecting to the M5Paper".into() } else { format!("Uploading {n}") }, f)));
            send(Msg::Uploaded(r));
        });
    }

    fn poll(&mut self, ctx: &egui::Context) {
        if let Ok(r) = self.rx_releases.try_recv() {
            self.releases = match r {
                Ok(v) => Releases::Ready(v),
                Err(e) => Releases::Failed(e),
            };
        }
        if let Some(rx) = &self.job {
            loop {
                match rx.try_recv() {
                    Ok(Msg::Progress(t, f)) => self.progress = (t, f),
                    Ok(Msg::Uploaded(r)) => {
                        self.upload_note = Some(r);
                        self.job = None;
                        break;
                    }
                    Ok(Msg::Done(r)) => {
                        self.outcome = Some(match r {
                            Ok(v) => Outcome::Ok(v),
                            Err(e) => Outcome::Err(e),
                        });
                        self.job = None;
                        break;
                    }
                    Err(TryRecvError::Empty) => break,
                    Err(TryRecvError::Disconnected) => {
                        self.outcome = Some(Outcome::Err(Error::Flash("the installer stopped unexpectedly".into())));
                        self.job = None;
                        break;
                    }
                }
            }
        }
        // Rescan the ports every second, but never while a flash is running.
        if self.job.is_none() && self.last_scan.is_none_or(|t| t.elapsed() >= Duration::from_secs(1)) {
            self.ports = device::scan();
            self.last_scan = Some(Instant::now());
        }
        ctx.request_repaint_after(Duration::from_millis(if self.job.is_some() { 100 } else { 1000 }));
    }

    /// Debug builds only: FLASHER_PREVIEW=nodevice|ready|installing|upload|done|error shows that card without a device.
    #[cfg(debug_assertions)]
    fn preview(mut self) -> Self {
        let Ok(state) = std::env::var("FLASHER_PREVIEW") else { return self };
        let rel = |t: &str| Release { tag_name: t.into(), draft: false, prerelease: false, assets: Vec::new() };
        self.releases = Releases::Ready(vec![rel("v1.1.0"), rel("v1.0.0")]);
        if state != "nodevice" {
            self.preview_port = Some("/dev/ttyUSB0".into());
        }
        let (tx, rx) = channel::<Msg>();
        std::mem::forget(tx);
        match state.as_str() {
            "installing" => {
                self.job = Some(rx);
                self.progress = ("Downloading".into(), Some(0.42));
            }
            "upload" => {
                self.job = Some(rx);
                self.last_upload = true;
                self.upload_names = vec!["secrets.yaml".into(), "config.yaml".into()];
                self.progress = ("Uploading config.yaml".into(), Some(0.6));
            }
            "done" => self.outcome = Some(Outcome::Ok("1.1.0".into())),
            "error" => {
                self.outcome = Some(Outcome::Err(Error::NoAnswer("Failed to connect to ESP32: timeout".into())));
                self.show_details = std::env::var("FLASHER_DETAILS").is_ok();
            }
            _ => {}
        }
        self
    }

    fn error(&self) -> Option<&Error> {
        match (&self.outcome, &self.upload_note) {
            (Some(Outcome::Err(e)), _) | (_, Some(Err(e))) => Some(e),
            _ => None,
        }
    }

    fn view(&self) -> View {
        let done = matches!(self.outcome, Some(Outcome::Ok(_))) || matches!(self.upload_note, Some(Ok(_)));
        pick_view(self.job.is_some(), self.error().is_some(), done, self.port().is_some())
    }

    fn can_install(&self) -> bool {
        self.job.is_none() && self.port().is_some() && (self.local.is_some() || self.selected_release().is_some())
    }

    /// Port picker, shown only when the port cannot be chosen automatically.
    fn port_ui(&mut self, ui: &mut egui::Ui) {
        let cands = device::candidates(&self.ports).len();
        if (cands == 0 && !self.ports.is_empty()) || cands > 1 {
            let shown = self.port().unwrap_or_else(|| "Choose a port".into());
            egui::ComboBox::from_id_salt("port").width(300.0).selected_text(shown).show_ui(ui, |ui| {
                for p in &self.ports {
                    ui.selectable_value(&mut self.chosen_port, Some(p.name.clone()), &p.name);
                }
            });
        }
    }

    fn version_ui(&mut self, ui: &mut egui::Ui, p: &Palette) {
        if let Some(path) = self.local.clone() {
            ui.label(ui::secondary(format!("Local files: {}", path.display()), 14.0, p));
            if ui::link(ui, "Use GitHub instead").clicked() {
                self.local = None;
            }
            return;
        }
        match &self.releases {
            Releases::Loading => {
                ui.label(ui::secondary("Looking for versions...", 16.0, p));
            }
            Releases::Failed(e) => {
                ui.label(e.message());
                ui.label(ui::secondary(e.action(), 14.0, p));
            }
            Releases::Ready(all) => {
                let tags: Vec<String> = releases::visible(all, self.show_pre).iter().map(|r| r.tag_name.clone()).collect();
                if tags.is_empty() {
                    ui.label("No releases found.");
                } else {
                    let current = self.selected_release().map(|r| r.tag_name).unwrap_or_default();
                    ui.horizontal(|ui| {
                        egui::ComboBox::from_id_salt("version").width(300.0).selected_text(format!("Version {current}")).show_ui(ui, |ui| {
                            for t in &tags {
                                ui.selectable_value(&mut self.chosen_tag, Some(t.clone()), t);
                            }
                        });
                        if ui::link(ui, "Choose file...").clicked() {
                            self.choose_file();
                        }
                    });
                }
                ui.checkbox(&mut self.show_pre, egui::RichText::new("Show pre-releases").size(14.0).color(p.secondary));
            }
        }
        if matches!(self.releases, Releases::Loading | Releases::Failed(_)) && ui::link(ui, "Choose file...").clicked() {
            self.choose_file();
        }
    }

    fn choose_file(&mut self) {
        let picked = rfd::FileDialog::new().set_title("Choose a release zip or its manifest.json").add_filter("Release", &["zip", "json"]).pick_file();
        if let Some(p) = picked {
            self.local = Some(if p.file_name().is_some_and(|n| n == "manifest.json") { p.parent().map(|d| d.to_path_buf()).unwrap_or(p) } else { p });
        }
    }

    fn content(&mut self, ui: &mut egui::Ui, view: View, p: &Palette) {
        // Header
        ui.horizontal(|ui| {
            ui::logo(ui, 36.0, p);
            ui.label(ui::bold("Inkhaus Installer", 20.0));
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                ui.label(ui::secondary(format!("v{}", env!("CARGO_PKG_VERSION")), 14.0, p));
            });
        });
        ui.add_space(6.0);
        ui::rule(ui, p);
        ui.add_space(14.0);

        let (mark, word) = match view {
            View::NoDevice => (Mark::Outline, "NO DEVICE"),
            View::Ready => (Mark::Filled, "CONNECTED"),
            View::Working => (Mark::Filled, "WORKING"),
            View::Done => (Mark::Filled, "DONE"),
            View::Error => (Mark::Crossed, "ERROR"),
        };
        ui.horizontal(|ui| {
            ui::mark(ui, mark, 20.0, p);
            ui.label(ui::bold(word, 14.0));
        });
        ui.add_space(4.0);

        match view {
            View::NoDevice => {
                ui.label(ui::bold("Plug in your M5Paper", 28.0));
                ui.label("Use a USB cable that carries data, not only power.");
                if cfg!(windows) && device::candidates(&self.ports).is_empty() {
                    if ui::link(ui, "Nothing appears? Install the USB driver").clicked() {
                        ui.ctx().open_url(egui::OpenUrl::new_tab("https://www.wch-ic.com/downloads/CH343SER_EXE.html"));
                    }
                }
                self.port_ui(ui);
            }
            View::Ready => {
                ui.label(ui::bold("M5Paper connected", 28.0));
                ui.label(ui::secondary(format!("Port {}", self.port().unwrap_or_default()), 16.0, p));
                self.port_ui(ui);
                self.version_ui(ui, p);
                ui.label("Your settings and SD card are kept.");
            }
            View::Working => {
                ui.label(ui::bold(if self.last_upload { "Upload YAML files" } else { "Installing" }, 28.0));
                let line = match self.progress.1 {
                    Some(f) => format!("{} {}%", self.progress.0, (f.clamp(0.0, 1.0) * 100.0).round() as u32),
                    None => format!("{}...", self.progress.0),
                };
                ui.label(ui::secondary(line, 16.0, p));
                ui.add_space(4.0);
                ui::block_bar(ui, self.progress.1, p);
                if self.last_upload {
                    let cur = self.upload_names.iter().position(|n| self.progress.0.ends_with(n.as_str()));
                    for (i, n) in self.upload_names.iter().enumerate() {
                        ui.horizontal(|ui| {
                            ui.spacing_mut().interact_size.y = 20.0;
                            ui::mark(ui, if cur.is_some_and(|c| i < c) { Mark::Filled } else { Mark::Outline }, 14.0, p);
                            ui.label(n);
                        });
                    }
                }
                ui.label("Do not unplug the M5Paper.");
            }
            View::Done => {
                ui.label(ui::bold("All done", 28.0));
                match &self.outcome {
                    Some(Outcome::Ok(v)) => ui.label(format!("Version {v} is installed and the M5Paper is restarting. Your settings were kept.")),
                    _ => ui.label("Uploaded. The M5Paper restarts with the new files."),
                };
            }
            View::Error => {
                ui.label(ui::bold("Something went wrong", 28.0));
                let (msg, action, raw) = self.error().map(|e| (e.message(), e.action(), format!("{e:?}"))).unwrap_or_default();
                ui.label(msg);
                ui.label(ui::secondary(action, 14.0, p));
                if ui::link(ui, if self.show_details { "Hide details v" } else { "Show details >" }).clicked() {
                    self.show_details = !self.show_details;
                }
                if self.show_details {
                    ui.add(egui::Label::new(egui::RichText::new(raw).monospace().color(p.secondary)).selectable(true));
                }
            }
        }
    }

    fn actions(&mut self, ui: &mut egui::Ui, view: View, p: &Palette) -> Option<Act> {
        let w = ui.available_width();
        let upload_w = 180.0;
        let gap = 14.0;
        let with_upload = matches!(view, View::Ready | View::Done);
        let main_w = if with_upload { w - upload_w - gap } else { w };
        let (label, enabled) = match view {
            View::NoDevice => ("Install", false),
            View::Ready => ("Install", self.can_install()),
            View::Working => ("Please wait", false),
            View::Done => ("Back", true),
            View::Error => ("Back", true),
        };
        let mut act = None;
        ui.horizontal(|ui| {
            ui.spacing_mut().item_spacing.x = gap;
            if ui::button(ui, label, Kind::Primary, enabled, Vec2::new(main_w, ui::BUTTON_H), 22.0, p) {
                act = Some(if matches!(view, View::Error | View::Done) { Act::Back } else { Act::Install });
            }
            if with_upload && ui::button(ui, "Upload YAML...", Kind::Secondary, self.port().is_some(), Vec2::new(upload_w, ui::BUTTON_H), 16.0, p) {
                act = Some(Act::Upload);
            }
        });
        // Enter presses the primary button unless a widget has focus.
        let enter = ui.ctx().input(|i| i.key_pressed(egui::Key::Enter)) && ui.ctx().memory(|m| m.focused().is_none());
        if enter && enabled && act.is_none() {
            act = Some(if matches!(view, View::Error | View::Done) { Act::Back } else { Act::Install });
        }
        act
    }
}

#[derive(Clone, Copy, PartialEq, Debug)]
pub enum View {
    NoDevice,
    Ready,
    Working,
    Done,
    Error,
}

enum Act {
    Install,
    Upload,
    Back,
}

/// Which card to show.
pub fn pick_view(working: bool, error: bool, done: bool, has_port: bool) -> View {
    if working {
        View::Working
    } else if error {
        View::Error
    } else if done {
        View::Done
    } else if has_port {
        View::Ready
    } else {
        View::NoDevice
    }
}

impl eframe::App for App {
    fn ui(&mut self, root: &mut egui::Ui, _frame: &mut eframe::Frame) {
        let ctx = root.ctx().clone();
        self.poll(&ctx);
        let p = ui::palette(ctx.theme());
        let view = self.view();
        if ctx.input(|i| i.key_pressed(egui::Key::Escape)) {
            self.show_details = false;
        }
        let side = ((root.available_width() - ui::COLUMN) / 2.0).max(ui::MARGIN);
        let margin = |top: f32, bottom: f32| egui::Frame::NONE.fill(p.bg).inner_margin(egui::Margin { left: side as i8, right: side as i8, top: top as i8, bottom: bottom as i8 });
        let mut act = None;
        egui::Panel::bottom("actions").frame(margin(8.0, 28.0)).show(root, |ui| {
            act = self.actions(ui, view, &p);
        });
        egui::CentralPanel::default().frame(margin(24.0, 8.0)).show(root, |ui| {
            egui::ScrollArea::vertical().auto_shrink([false, false]).show(ui, |ui| self.content(ui, view, &p));
        });
        match act {
            Some(Act::Install) => self.start(&ctx),
            Some(Act::Upload) => self.start_upload(&ctx),
            Some(Act::Back) => {
                self.outcome = None;
                self.upload_note = None;
                self.show_details = false;
            }
            None => {}
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn view_priority() {
        assert_eq!(pick_view(true, true, true, true), View::Working);
        assert_eq!(pick_view(false, true, true, true), View::Error);
        assert_eq!(pick_view(false, false, true, false), View::Done);
        assert_eq!(pick_view(false, false, false, true), View::Ready);
        assert_eq!(pick_view(false, false, false, false), View::NoDevice);
    }
}
