//! The window.

use crate::device::{self, PortEntry};
use crate::error::Error;
use crate::flash::{self, Phase};
use crate::releases::{self, Release};
use crate::source;
use eframe::egui;
use std::path::PathBuf;
use std::sync::mpsc::{channel, Receiver, TryRecvError};
use std::time::{Duration, Instant};

enum Releases {
    Loading,
    Ready(Vec<Release>),
    Failed(Error),
}

enum Msg {
    Progress(String, f32),
    Done(Result<String, Error>),
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
    progress: (String, f32),
    outcome: Option<Outcome>,
}

impl App {
    pub fn new() -> Self {
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
            progress: (String::new(), 0.0),
            outcome: None,
        }
    }

    /// The port to use: the only candidate, else the user's choice.
    fn port(&self) -> Option<String> {
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
        self.progress = ("Downloading".into(), 0.0);
        let ctx = ctx.clone();
        std::thread::spawn(move || {
            let send = |m: Msg| {
                let _ = tx.send(m);
                ctx.request_repaint();
            };
            let result = (|| -> Result<String, Error> {
                let tmp = tempfile::tempdir()?;
                if let Some(p) = &local {
                    send(Msg::Progress("Preparing".into(), 0.0));
                    source::import_local(p, tmp.path())?;
                } else if let Some(r) = &release {
                    source::fetch_release(r, tmp.path(), &mut |f| send(Msg::Progress("Downloading".into(), f)))?;
                }
                send(Msg::Progress("Checking files".into(), 0.0));
                let image = source::load_dir(tmp.path())?;
                flash::flash(&port, &image, &mut |ph: Phase, f| send(Msg::Progress(ph.text().into(), f)))?;
                Ok(image.manifest.version)
            })();
            send(Msg::Done(result));
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

    fn device_ui(&mut self, ui: &mut egui::Ui) {
        let cands = device::candidates(&self.ports).len();
        match (cands, self.port()) {
            (1, Some(p)) => {
                ui.label(egui::RichText::new(format!("M5Paper connected ({p})")).size(18.0).color(egui::Color32::from_rgb(0, 130, 0)));
            }
            _ => {
                ui.label(egui::RichText::new("Plug in the M5Paper with a USB cable").size(18.0));
                if cfg!(windows) && cands == 0 {
                    ui.hyperlink_to("Nothing appears? Install the USB driver", "https://www.wch-ic.com/downloads/CH343SER_EXE.html");
                }
            }
        }
        if (cands == 0 && !self.ports.is_empty()) || cands > 1 {
            let shown = self.port().unwrap_or_else(|| "Choose a port".into());
            egui::ComboBox::from_label("Port").selected_text(shown).show_ui(ui, |ui| {
                for p in &self.ports {
                    ui.selectable_value(&mut self.chosen_port, Some(p.name.clone()), &p.name);
                }
            });
        }
    }

    fn version_ui(&mut self, ui: &mut egui::Ui, busy: bool) {
        ui.add_enabled_ui(!busy, |ui| {
            if let Some(p) = self.local.clone() {
                ui.horizontal(|ui| {
                    ui.label(format!("Local files: {}", p.display()));
                    if ui.button("Use GitHub instead").clicked() {
                        self.local = None;
                    }
                });
                return;
            }
            match &self.releases {
                Releases::Loading => {
                    ui.label("Looking for versions...");
                }
                Releases::Failed(e) => {
                    ui.colored_label(egui::Color32::from_rgb(170, 0, 0), e.message());
                    ui.label(e.action());
                }
                Releases::Ready(all) => {
                    let list = releases::visible(all, self.show_pre);
                    if list.is_empty() {
                        ui.label("No releases found.");
                    } else {
                        let current = self.selected_release().map(|r| r.tag_name).unwrap_or_default();
                        egui::ComboBox::from_label("Version").selected_text(current).show_ui(ui, |ui| {
                            for r in &list {
                                ui.selectable_value(&mut self.chosen_tag, Some(r.tag_name.clone()), &r.tag_name);
                            }
                        });
                    }
                    ui.checkbox(&mut self.show_pre, "Show pre-releases");
                }
            }
            if ui.button("Choose file...").clicked() {
                let picked = rfd::FileDialog::new()
                    .set_title("Choose a release zip or its manifest.json")
                    .add_filter("Release", &["zip", "json"])
                    .pick_file();
                self.local = picked.map(|p| if p.file_name().is_some_and(|n| n == "manifest.json") { p.parent().map(|d| d.to_path_buf()).unwrap_or(p) } else { p });
            }
        });
    }
}

impl eframe::App for App {
    fn ui(&mut self, root: &mut egui::Ui, _frame: &mut eframe::Frame) {
        let ctx = root.ctx().clone();
        self.poll(&ctx);
        let busy = self.job.is_some();
        egui::CentralPanel::default().show(root, |ui| {
            ui.add_space(8.0);
            ui.heading("M5Paper Installer");
            ui.add_space(12.0);
            self.device_ui(ui);
            ui.add_space(12.0);
            self.version_ui(ui, busy);
            ui.add_space(16.0);

            let can = !busy && self.port().is_some() && (self.local.is_some() || self.selected_release().is_some());
            let btn = egui::Button::new(egui::RichText::new("Install").size(28.0)).min_size(egui::vec2(ui.available_width(), 64.0));
            if ui.add_enabled(can, btn).clicked() {
                self.start(&ctx);
            }
            ui.add_space(12.0);
            if busy {
                ui.label(format!("{}...", self.progress.0));
                ui.add(egui::ProgressBar::new(self.progress.1).show_percentage());
                ui.label("Do not unplug the M5Paper.");
            }
            match &self.outcome {
                Some(Outcome::Ok(v)) => {
                    ui.label(egui::RichText::new(format!("Done. Version {v} is installed and the M5Paper is restarting. Your settings were kept.")).size(16.0).color(egui::Color32::from_rgb(0, 130, 0)));
                }
                Some(Outcome::Err(e)) => {
                    ui.label(egui::RichText::new(e.message()).size(16.0).color(egui::Color32::from_rgb(170, 0, 0)));
                    ui.label(e.action());
                }
                None => {}
            }
        });
    }
}
