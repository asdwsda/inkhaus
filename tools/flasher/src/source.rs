//! Getting the firmware files into a temp dir (download, local folder or zip) and verifying them.

use crate::error::Error;
use crate::manifest::{self, Manifest};
use crate::releases::{agent, Release};
use std::io::{Read, Write};
use std::path::Path;

const MAX_FILE: u64 = 64 * 1024 * 1024;

/// Verified firmware, ready to flash.
pub struct Image {
    pub manifest: Manifest,
    /// (flash offset, bytes), in manifest order.
    pub segments: Vec<(u32, Vec<u8>)>,
}

impl Image {
    pub fn total_bytes(&self) -> usize {
        self.segments.iter().map(|s| s.1.len()).sum()
    }
}

/// Read manifest.json and every listed file from `dir` and verify them.
pub fn load_dir(dir: &Path) -> Result<Image, Error> {
    let text = std::fs::read_to_string(dir.join("manifest.json"))
        .map_err(|_| Error::BadManifest("there is no manifest.json in it".into()))?;
    let manifest = manifest::parse(&text)?;
    let mut segments = Vec::new();
    for seg in &manifest.segments {
        let data = std::fs::read(dir.join(&seg.file)).map_err(|_| Error::BadFile(format!("{} is missing", seg.file)))?;
        manifest::verify(seg, &data)?;
        segments.push((seg.offset, data));
    }
    Ok(Image { manifest, segments })
}

fn download(url: &str, dest: &Path, on_bytes: &mut dyn FnMut(usize)) -> Result<(), Error> {
    let mut resp = agent().get(url).header("User-Agent", "m5paper-flasher").call()?;
    let mut reader = resp.body_mut().with_config().limit(MAX_FILE).reader();
    let mut out = std::fs::File::create(dest)?;
    let mut buf = [0u8; 32 * 1024];
    loop {
        let n = reader.read(&mut buf).map_err(|e| Error::Network(e.to_string()))?;
        if n == 0 {
            break;
        }
        out.write_all(&buf[..n])?;
        on_bytes(n);
    }
    Ok(())
}

/// Download manifest.json and its files for `release` into `dir`; `on_progress(fraction)` is 0..1.
pub fn fetch_release(release: &Release, dir: &Path, on_progress: &mut dyn FnMut(f32)) -> Result<(), Error> {
    let url = release.asset_url("manifest.json").ok_or_else(|| Error::BadManifest("it has no manifest.json".into()))?;
    let manifest_path = dir.join("manifest.json");
    download(url, &manifest_path, &mut |_| {})?;
    let manifest = manifest::parse(&std::fs::read_to_string(&manifest_path)?)?;
    let total: u64 = manifest.segments.iter().map(|s| s.size).sum();
    let mut done = 0u64;
    for seg in &manifest.segments {
        let url = release
            .asset_url(&seg.file)
            .ok_or_else(|| Error::BadManifest(format!("{} is not attached to the release", seg.file)))?;
        download(url, &dir.join(&seg.file), &mut |n| {
            done += n as u64;
            on_progress((done as f32 / total as f32).min(1.0));
        })?;
    }
    Ok(())
}

/// Copy a local release (a folder, or a zip containing manifest.json) into `dir`.
pub fn import_local(path: &Path, dir: &Path) -> Result<(), Error> {
    if path.is_dir() {
        for entry in std::fs::read_dir(path)? {
            let p = entry?.path();
            if p.is_file() {
                std::fs::copy(&p, dir.join(p.file_name().unwrap()))?;
            }
        }
        return Ok(());
    }
    let mut zip = zip::ZipArchive::new(std::fs::File::open(path)?)
        .map_err(|_| Error::BadManifest("that file is not a zip archive or a release folder".into()))?;
    for i in 0..zip.len() {
        let mut f = zip.by_index(i).map_err(|e| Error::Io(e.to_string()))?;
        if !f.is_file() {
            continue;
        }
        // Flatten: only the base name is used, so nothing can escape `dir`.
        let Some(name) = f.enclosed_name().and_then(|p| p.file_name().map(|n| n.to_owned())) else { continue };
        let mut out = std::fs::File::create(dir.join(name))?;
        std::io::copy(&mut (&mut f).take(MAX_FILE), &mut out)?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use sha2::{Digest, Sha256};

    fn write_release(dir: &Path, corrupt: bool) {
        let files: [(&str, u32, &[u8]); 2] = [("a.bin", 0x1000, b"aaaa"), ("b.bin", 0x8000, b"bbbbbbbb")];
        let mut segs = Vec::new();
        for (name, off, data) in files {
            let h = hex::encode(Sha256::digest(data));
            segs.push(format!(r#"{{"file":"{name}","offset":{off},"size":{},"sha256":"{h}"}}"#, data.len()));
            let written: &[u8] = if corrupt && name == "b.bin" { b"bbbbbbbX" } else { data };
            std::fs::write(dir.join(name), written).unwrap();
        }
        let m = format!(r#"{{"version":"9.9.9","chip":"esp32","flash_size":"16MB","segments":[{}]}}"#, segs.join(","));
        std::fs::write(dir.join("manifest.json"), m).unwrap();
    }

    #[test]
    fn loads_and_verifies_a_folder() {
        let d = tempfile::tempdir().unwrap();
        write_release(d.path(), false);
        let img = load_dir(d.path()).unwrap();
        assert_eq!(img.manifest.version, "9.9.9");
        assert_eq!(img.segments[1].0, 0x8000);
        assert_eq!(img.total_bytes(), 12);
    }

    #[test]
    fn wrong_hash_is_reported() {
        let d = tempfile::tempdir().unwrap();
        write_release(d.path(), true);
        assert_eq!(load_dir(d.path()).err(), Some(Error::HashMismatch("b.bin".into())));
    }

    #[test]
    fn missing_file_and_missing_manifest() {
        let d = tempfile::tempdir().unwrap();
        write_release(d.path(), false);
        std::fs::remove_file(d.path().join("a.bin")).unwrap();
        assert!(matches!(load_dir(d.path()), Err(Error::BadFile(_))));
        let e = tempfile::tempdir().unwrap();
        assert!(matches!(load_dir(e.path()), Err(Error::BadManifest(_))));
    }

    #[test]
    fn imports_zip_with_subfolder() {
        let src = tempfile::tempdir().unwrap();
        write_release(src.path(), false);
        let zpath = src.path().join("rel.zip");
        {
            let mut w = zip::ZipWriter::new(std::fs::File::create(&zpath).unwrap());
            let opts = zip::write::SimpleFileOptions::default();
            for n in ["manifest.json", "a.bin", "b.bin"] {
                w.start_file(format!("release/{n}"), opts).unwrap();
                w.write_all(&std::fs::read(src.path().join(n)).unwrap()).unwrap();
            }
            w.finish().unwrap();
        }
        let dst = tempfile::tempdir().unwrap();
        import_local(&zpath, dst.path()).unwrap();
        assert_eq!(load_dir(dst.path()).unwrap().segments.len(), 2);
        // plain folder too
        let dst2 = tempfile::tempdir().unwrap();
        import_local(src.path(), dst2.path()).unwrap();
        assert!(load_dir(dst2.path()).is_ok());
        // not a zip
        let junk = src.path().join("junk.txt");
        std::fs::write(&junk, "x").unwrap();
        assert!(import_local(&junk, dst.path()).is_err());
    }
}
