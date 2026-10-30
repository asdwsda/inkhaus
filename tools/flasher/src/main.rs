mod app;
mod device;
mod error;
mod flash;
mod manifest;
mod provision;
mod releases;
mod source;

use std::path::PathBuf;

const USAGE: &str = "M5Paper flasher\n\nUsage:\n  flasher                       open the window\n  flasher --dry-run [options]   resolve a release and verify its files, no device needed\n\nOptions:\n  --dir PATH         use a local release folder or zip\n  --version TAG      release tag (default: latest stable)\n  --prerelease       allow pre-releases when choosing the default\n  --repo OWNER/NAME  GitHub repository (default: the project repo)\n  --releases FILE    read the release list from a JSON file instead of GitHub";

fn dry_run(args: &[String]) -> Result<(), error::Error> {
    let get = |flag: &str| args.iter().position(|a| a == flag).and_then(|i| args.get(i + 1)).cloned();
    let tmp = tempfile::tempdir()?;
    if let Some(dir) = get("--dir") {
        source::import_local(&PathBuf::from(dir), tmp.path())?;
    } else {
        let list = match get("--releases") {
            Some(f) => releases::parse(&std::fs::read_to_string(f)?)?,
            None => releases::fetch(&get("--repo").unwrap_or_else(|| releases::DEFAULT_REPO.into()))?,
        };
        let shown = releases::visible(&list, args.iter().any(|a| a == "--prerelease") || get("--version").is_some());
        let rel = match get("--version") {
            Some(v) => shown.into_iter().find(|r| r.tag_name == v).ok_or_else(|| error::Error::Http(404))?,
            None => *shown.get(releases::default_index(&shown)).ok_or_else(|| error::Error::BadManifest("no release found".into()))?,
        };
        println!("release: {}", rel.tag_name);
        source::fetch_release(rel, tmp.path(), &mut |_| {})?;
    }
    let img = source::load_dir(tmp.path())?;
    println!("version: {}  chip: {}  flash: {}", img.manifest.version, img.manifest.chip, img.manifest.flash_size);
    for (seg, (off, data)) in img.manifest.segments.iter().zip(&img.segments) {
        println!("  0x{off:06x}  {:>8} bytes  {}  sha256 ok", data.len(), seg.file);
    }
    println!("OK: {} bytes verified, nothing written", img.total_bytes());
    Ok(())
}

fn main() -> eframe::Result<()> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    if args.iter().any(|a| a == "--help" || a == "-h") {
        println!("{USAGE}");
        return Ok(());
    }
    if args.iter().any(|a| a == "--dry-run") {
        if let Err(e) = dry_run(&args) {
            eprintln!("ERROR: {e}");
            std::process::exit(1);
        }
        return Ok(());
    }
    let opts = eframe::NativeOptions {
        viewport: eframe::egui::ViewportBuilder::default().with_inner_size([520.0, 420.0]).with_title("M5Paper Installer"),
        ..Default::default()
    };
    eframe::run_native("M5Paper Installer", opts, Box::new(|_cc| Ok(Box::new(app::App::new()))))
}
