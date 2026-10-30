//! Sending config.yaml / secrets.yaml to the SD card over the task 020 serial protocol
//! (port of tools/provision.py). File contents are never logged or put in error texts.
//!
//! Protocol: `putfile <name> <size> <crc32 hex8>\n` -> `PUT-READY`, raw bytes with a `PUT-ACK <offset>`
//! after every 1 KB (except at the end), then `PUT-OK <size> <crc>` / `PUT-ERR ...` and a
//! `PUT-CONFIG ...` / `PUT-SECRETS ...` summary. `reboot\n` restarts the device.

use crate::error::Error;
use std::io::{Read, Write};
use std::time::{Duration, Instant};

pub const MAX_BYTES: usize = 64 * 1024;
const ACK_EVERY: usize = 1024;
const PIECE: usize = 256;

/// Timeouts, shortened in tests.
#[derive(Clone, Copy)]
pub struct Timeouts {
    pub ready: Duration,
    pub ack: Duration,
    pub result: Duration,
    pub summary: Duration,
    pub attempts: u32,
}

impl Default for Timeouts {
    fn default() -> Self {
        Timeouts { ready: Duration::from_secs(4), ack: Duration::from_secs(5), result: Duration::from_secs(15), summary: Duration::from_secs(5), attempts: 4 }
    }
}

pub fn crc32(data: &[u8]) -> u32 {
    let mut crc = 0xFFFF_FFFFu32;
    for &b in data {
        crc ^= b as u32;
        for _ in 0..8 {
            crc = if crc & 1 != 0 { (crc >> 1) ^ 0xEDB8_8320 } else { crc >> 1 };
        }
    }
    !crc
}

/// Only these names are accepted by the device (test-*.yaml is for developers, not offered here).
pub fn valid_name(name: &str) -> bool {
    name == "config.yaml" || name == "secrets.yaml"
}

pub fn command_line(name: &str, size: usize, crc: u32) -> String {
    format!("putfile {name} {size} {crc:08x}\n")
}

/// Parse `PUT-OK <size> <crc>`.
pub fn parse_ok(line: &str) -> Option<(usize, u32)> {
    let mut it = line.strip_prefix("PUT-OK ")?.split_whitespace();
    let size = it.next()?.parse().ok()?;
    let crc = u32::from_str_radix(it.next()?, 16).ok()?;
    Some((size, crc))
}

fn fail(msg: impl Into<String>) -> Error {
    Error::Flash(msg.into())
}

fn read_line<P: Read>(p: &mut P, deadline: Instant) -> Option<String> {
    let mut buf = Vec::new();
    let mut one = [0u8; 1];
    while Instant::now() < deadline {
        match p.read(&mut one) {
            Ok(1) => {
                if one[0] == b'\n' {
                    return Some(String::from_utf8_lossy(&buf).trim().to_string());
                }
                buf.push(one[0]);
            }
            Ok(_) => std::thread::sleep(Duration::from_millis(2)),
            Err(e) if e.kind() == std::io::ErrorKind::TimedOut || e.kind() == std::io::ErrorKind::Interrupted => {}
            Err(_) => return None,
        }
    }
    None
}

/// Read lines until one starts with one of `prefixes`.
fn wait_for<P: Read>(p: &mut P, prefixes: &[&str], timeout: Duration) -> Option<String> {
    let deadline = Instant::now() + timeout;
    while let Some(line) = read_line(p, deadline) {
        if prefixes.iter().any(|x| line.starts_with(x)) {
            return Some(line);
        }
    }
    None
}

/// Send one file on an already booted device. Returns the device's summary line (no file content).
pub fn transfer<P: Read + Write>(p: &mut P, name: &str, data: &[u8], t: Timeouts, progress: &mut dyn FnMut(f32)) -> Result<String, Error> {
    if !valid_name(name) {
        return Err(fail("only config.yaml and secrets.yaml can be uploaded"));
    }
    if data.is_empty() {
        return Err(fail(format!("{name} is empty")));
    }
    if data.len() > MAX_BYTES {
        return Err(fail(format!("{name} is larger than 64 KB")));
    }
    let size = data.len();
    let crc = crc32(data);

    let mut resp = None;
    for _ in 0..t.attempts {
        let _ = p.write_all(command_line(name, size, crc).as_bytes());
        let _ = p.flush();
        resp = wait_for(p, &["PUT-READY", "PUT-ERR"], t.ready);
        if resp.is_some() {
            break;
        }
    }
    match resp {
        None => return Err(Error::NoAnswer("no answer to putfile".into())),
        Some(r) if r.starts_with("PUT-ERR") => return Err(fail(format!("the M5Paper refused {name} ({r})"))),
        _ => {}
    }

    let (mut sent, mut next_ack) = (0usize, ACK_EVERY);
    while sent < size {
        let end = (sent + PIECE).min(size).min(next_ack);
        p.write_all(&data[sent..end]).map_err(|e| Error::Io(e.to_string()))?;
        sent = end;
        if sent == next_ack && sent < size {
            match wait_for(p, &["PUT-ACK", "PUT-ERR"], t.ack) {
                Some(a) if a.starts_with("PUT-ACK") => {}
                Some(a) => return Err(fail(format!("transfer stopped by the M5Paper ({a})"))),
                None => return Err(Error::NoAnswer("no PUT-ACK".into())),
            }
            next_ack += ACK_EVERY;
        }
        progress(sent as f32 / size as f32);
    }
    let _ = p.flush();

    let ok = wait_for(p, &["PUT-OK", "PUT-ERR"], t.result).ok_or_else(|| Error::NoAnswer("no PUT-OK".into()))?;
    if ok.starts_with("PUT-ERR") {
        return Err(fail(format!("the M5Paper rejected {name} ({ok})")));
    }
    match parse_ok(&ok) {
        Some((s, c)) if s == size && c == crc => {}
        _ => return Err(fail(format!("the M5Paper received {name} differently (checksum mismatch)"))),
    }
    let tag = if name == "secrets.yaml" { "PUT-SECRETS" } else { "PUT-CONFIG" };
    Ok(wait_for(p, &[tag], t.summary).unwrap_or_else(|| ok.clone()))
}

pub fn reboot<P: Write>(p: &mut P) -> Result<(), Error> {
    p.write_all(b"reboot\n").and_then(|_| p.flush()).map_err(|e| Error::Io(e.to_string()))
}

/// Open the port, reset the board (DTR+RTS together) and wait for the end of its boot.
pub fn open(port: &str) -> Result<Box<dyn serialport::SerialPort>, Error> {
    let mut s = serialport::new(port, 115_200).timeout(Duration::from_millis(100)).open().map_err(|e| Error::from_flash_text(port, &e.to_string()))?;
    // A board in deep sleep ignores the port; DTR+RTS asserted together then released resets it.
    let _ = s.write_data_terminal_ready(true);
    let _ = s.write_request_to_send(true);
    std::thread::sleep(Duration::from_millis(200));
    let _ = s.write_data_terminal_ready(false);
    let _ = s.write_request_to_send(false);
    let _ = s.clear(serialport::ClearBuffer::Input);
    let mut deadline = Instant::now() + Duration::from_secs(4);
    let mut seen_boot = false;
    while let Some(line) = read_line(&mut s, deadline) {
        if line.contains("BOOT:") {
            seen_boot = true;
            deadline = Instant::now() + Duration::from_secs(25);
            if line.contains("settle_bst_end") {
                break;
            }
        }
    }
    if seen_boot {
        std::thread::sleep(Duration::from_millis(300));
    }
    let _ = s.clear(serialport::ClearBuffer::Input);
    Ok(s)
}

/// Upload several files in one session. Returns one "name: summary" line per file.
pub fn upload_files(port: &str, files: &[(String, Vec<u8>)], progress: &mut dyn FnMut(&str, f32)) -> Result<Vec<String>, Error> {
    let mut s = open(port)?;
    let mut out = Vec::new();
    for (name, data) in files {
        let summary = transfer(&mut s, name, data, Timeouts::default(), &mut |f| progress(name, f))?;
        out.push(format!("{name}: {summary}"));
    }
    Ok(out)
}

pub fn restart(port: &str) -> Result<(), Error> {
    // Opening resets the board already, but the user asked for a restart: be explicit and wait for no boot.
    let mut s = serialport::new(port, 115_200).timeout(Duration::from_millis(100)).open().map_err(|e| Error::from_flash_text(port, &e.to_string()))?;
    reboot(&mut s)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::VecDeque;

    /// A scripted device following the firmware's protocol.
    struct Fake {
        out: VecDeque<u8>,
        cmd: Vec<u8>,
        expect: usize,
        got: Vec<u8>,
        writes_seen: Vec<String>,
        corrupt_crc: bool,
        silent: bool,
    }

    impl Fake {
        fn new() -> Self {
            Fake { out: VecDeque::new(), cmd: vec![], expect: 0, got: vec![], writes_seen: vec![], corrupt_crc: false, silent: false }
        }
        fn say(&mut self, s: &str) {
            self.out.extend(s.bytes());
            self.out.push_back(b'\n');
        }
    }

    impl Read for Fake {
        fn read(&mut self, b: &mut [u8]) -> std::io::Result<usize> {
            match self.out.pop_front() {
                Some(c) => {
                    b[0] = c;
                    Ok(1)
                }
                None => Err(std::io::ErrorKind::TimedOut.into()),
            }
        }
    }

    impl Write for Fake {
        fn write(&mut self, b: &[u8]) -> std::io::Result<usize> {
            if self.silent {
                return Ok(b.len());
            }
            if self.expect == 0 {
                self.cmd.extend_from_slice(b);
                if self.cmd.ends_with(b"\n") {
                    let line = String::from_utf8(std::mem::take(&mut self.cmd)).unwrap();
                    self.writes_seen.push(line.trim().into());
                    let f: Vec<&str> = line.split_whitespace().collect();
                    if f[0] == "putfile" {
                        self.expect = f[2].parse().unwrap();
                        self.say("PUT-READY");
                    }
                }
            } else {
                self.got.extend_from_slice(b);
                if self.got.len() >= self.expect {
                    let crc = if self.corrupt_crc { 0 } else { crc32(&self.got) };
                    let n = self.got.len();
                    self.expect = 0;
                    self.say(&format!("PUT-OK {n} {crc:08x}"));
                    self.say("PUT-CONFIG ok tabs=1");
                } else if self.got.len() % ACK_EVERY == 0 {
                    let n = self.got.len();
                    self.say(&format!("PUT-ACK {n}"));
                }
            }
            Ok(b.len())
        }
        fn flush(&mut self) -> std::io::Result<()> {
            Ok(())
        }
    }

    fn fast() -> Timeouts {
        Timeouts { ready: Duration::from_millis(30), ack: Duration::from_millis(30), result: Duration::from_millis(30), summary: Duration::from_millis(30), attempts: 2 }
    }

    #[test]
    fn crc_matches_zlib() {
        assert_eq!(crc32(b"123456789"), 0xCBF4_3926);
        assert_eq!(crc32(b""), 0);
    }

    #[test]
    fn command_framing() {
        assert_eq!(command_line("config.yaml", 12, 0xabc), "putfile config.yaml 12 00000abc\n");
        assert_eq!(parse_ok("PUT-OK 12 00000abc"), Some((12, 0xabc)));
        assert_eq!(parse_ok("PUT-ERR crc"), None);
    }

    #[test]
    fn names() {
        assert!(valid_name("config.yaml") && valid_name("secrets.yaml"));
        assert!(!valid_name("test-x.yaml") && !valid_name("../config.yaml") && !valid_name("a.yaml"));
    }

    #[test]
    fn round_trip_with_acks() {
        let data: Vec<u8> = (0..2500u32).map(|i| (i % 251) as u8).collect();
        let mut d = Fake::new();
        let mut last = 0.0;
        let s = transfer(&mut d, "config.yaml", &data, fast(), &mut |f| last = f).unwrap();
        assert_eq!(s, "PUT-CONFIG ok tabs=1");
        assert_eq!(d.got, data);
        assert_eq!(d.writes_seen[0], format!("putfile config.yaml 2500 {:08x}", crc32(&data)));
        assert_eq!(last, 1.0);
    }

    #[test]
    fn exact_kb_has_no_trailing_ack_wait() {
        let data = vec![b'a'; 1024];
        let mut d = Fake::new();
        assert!(transfer(&mut d, "secrets.yaml", &data, fast(), &mut |_| {}).is_ok());
    }

    #[test]
    fn crc_mismatch_is_an_error() {
        let mut d = Fake::new();
        d.corrupt_crc = true;
        assert!(transfer(&mut d, "config.yaml", b"a: 1\n", fast(), &mut |_| {}).is_err());
    }

    #[test]
    fn silent_device_is_no_answer() {
        let mut d = Fake::new();
        d.silent = true;
        assert!(matches!(transfer(&mut d, "config.yaml", b"a: 1\n", fast(), &mut |_| {}), Err(Error::NoAnswer(_))));
    }

    #[test]
    fn refuses_bad_input_and_never_echoes_content() {
        let mut d = Fake::new();
        assert!(transfer(&mut d, "x.yaml", b"a", fast(), &mut |_| {}).is_err());
        assert!(transfer(&mut d, "config.yaml", b"", fast(), &mut |_| {}).is_err());
        assert!(transfer(&mut d, "config.yaml", &vec![b'a'; MAX_BYTES + 1], fast(), &mut |_| {}).is_err());
        let mut d = Fake::new();
        d.corrupt_crc = true;
        let e = transfer(&mut d, "secrets.yaml", b"token: SECRETVALUE\n", fast(), &mut |_| {}).unwrap_err();
        assert!(!e.to_string().contains("SECRETVALUE"));
    }

    #[test]
    fn reboot_command() {
        let mut d = Fake::new();
        reboot(&mut d).unwrap();
        assert_eq!(d.writes_seen, vec!["reboot"]);
    }
}
