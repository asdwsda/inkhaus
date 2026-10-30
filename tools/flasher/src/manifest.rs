//! manifest.json (task 061) parsing and hash verification.

use crate::error::Error;
use serde::Deserialize;
use sha2::{Digest, Sha256};

#[derive(Debug, Clone, Deserialize, PartialEq)]
pub struct Segment {
    pub file: String,
    pub offset: u32,
    pub size: u64,
    pub sha256: String,
}

#[derive(Debug, Clone, Deserialize, PartialEq)]
pub struct Manifest {
    pub version: String,
    pub chip: String,
    #[serde(default)]
    pub flash_size: String,
    pub segments: Vec<Segment>,
}

fn bad(msg: impl Into<String>) -> Error {
    Error::BadManifest(msg.into())
}

pub fn parse(text: &str) -> Result<Manifest, Error> {
    let m: Manifest = serde_json::from_str(text).map_err(|e| bad(format!("manifest.json is not valid ({e})")))?;
    validate(&m)?;
    Ok(m)
}

fn validate(m: &Manifest) -> Result<(), Error> {
    if m.chip != "esp32" {
        return Err(bad(format!("it is for the chip \"{}\", not for the M5Paper", m.chip)));
    }
    if m.segments.is_empty() {
        return Err(bad("the manifest lists no files"));
    }
    for s in &m.segments {
        if s.file.is_empty() || s.file.contains(['/', '\\']) || s.file.contains("..") {
            return Err(bad(format!("unsafe file name \"{}\"", s.file)));
        }
        if s.sha256.len() != 64 || !s.sha256.chars().all(|c| c.is_ascii_hexdigit()) {
            return Err(bad(format!("bad checksum for {}", s.file)));
        }
        if s.size == 0 {
            return Err(bad(format!("{} is empty", s.file)));
        }
    }
    let mut sorted: Vec<&Segment> = m.segments.iter().collect();
    sorted.sort_by_key(|s| s.offset);
    for w in sorted.windows(2) {
        if u64::from(w[0].offset) + w[0].size > u64::from(w[1].offset) {
            return Err(bad(format!("{} and {} overlap", w[0].file, w[1].file)));
        }
    }
    Ok(())
}

/// Check size and SHA-256 of a segment's bytes.
pub fn verify(seg: &Segment, data: &[u8]) -> Result<(), Error> {
    if data.len() as u64 != seg.size {
        return Err(Error::BadFile(format!("{} has {} bytes, expected {}", seg.file, data.len(), seg.size)));
    }
    let digest = hex::encode(Sha256::digest(data));
    if !digest.eq_ignore_ascii_case(&seg.sha256) {
        return Err(Error::HashMismatch(seg.file.clone()));
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    pub const EXAMPLE: &str = include_str!("../tests/fixtures/manifest.json");

    #[test]
    fn parses_example() {
        let m = parse(EXAMPLE).unwrap();
        assert_eq!(m.version, "1.0.0");
        assert_eq!(m.segments.len(), 4);
        assert_eq!(m.segments[3].file, "firmware.bin");
        assert_eq!(m.segments[3].offset, 0x10000);
        assert_eq!(m.segments[0].offset, 0x1000);
    }

    #[test]
    fn rejects_bad_manifests() {
        let wrong_chip = EXAMPLE.replace("\"esp32\"", "\"esp32s3\"");
        assert!(parse(&wrong_chip).is_err());
        let traversal = EXAMPLE.replace("firmware.bin", "../firmware.bin");
        assert!(parse(&traversal).is_err());
        assert!(parse("{}").is_err());
        assert!(parse("not json").is_err());
        let overlap = EXAMPLE.replace("\"offset\": 32768", "\"offset\": 4100");
        assert!(parse(&overlap).is_err());
    }

    #[test]
    fn verifies_hash_and_size() {
        let data = b"hello";
        let seg = Segment {
            file: "a.bin".into(),
            offset: 0,
            size: 5,
            sha256: "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824".into(),
        };
        assert!(verify(&seg, data).is_ok());
        assert_eq!(verify(&seg, b"hellp"), Err(Error::HashMismatch("a.bin".into())));
        assert!(matches!(verify(&seg, b"hell"), Err(Error::BadFile(_))));
    }
}
