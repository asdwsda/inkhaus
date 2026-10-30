//! Errors with plain-language text and one suggested action.

use std::fmt;

#[derive(Debug, Clone, PartialEq)]
pub enum Error {
    /// No network / DNS / connection failure.
    Offline,
    /// GitHub refused (rate limit) or answered with an unexpected status.
    Http(u16),
    /// Any other network problem.
    Network(String),
    /// The release or the local folder has no usable manifest.
    BadManifest(String),
    /// A downloaded file does not match the manifest.
    HashMismatch(String),
    /// A file listed in the manifest is missing or has the wrong size.
    BadFile(String),
    /// No device selected / plugged in.
    NoDevice,
    /// The serial port cannot be opened (another program uses it, or no permission).
    PortBusy(String),
    /// The chip did not answer.
    NoAnswer(String),
    /// Anything else from the flasher.
    Flash(String),
    Io(String),
}

impl Error {
    /// What happened, in plain words.
    pub fn message(&self) -> String {
        match self {
            Error::Offline => "Could not reach GitHub. There seems to be no internet connection.".into(),
            Error::Http(403) | Error::Http(429) => {
                "GitHub is limiting requests from this network right now.".into()
            }
            Error::Http(404) => "This release was not found on GitHub.".into(),
            Error::Http(code) => format!("GitHub answered with an error (HTTP {code})."),
            Error::Network(e) => format!("The download failed ({e})."),
            Error::BadManifest(e) => format!("This release cannot be installed: {e}."),
            Error::HashMismatch(f) => {
                format!("The downloaded file {f} is damaged (its checksum does not match). Nothing was written to the device.")
            }
            Error::BadFile(e) => format!("A firmware file is missing or the wrong size: {e}. Nothing was written to the device."),
            Error::NoDevice => "No M5Paper found.".into(),
            Error::PortBusy(p) => format!("The USB port {p} could not be opened."),
            Error::NoAnswer(_) => "The M5Paper did not answer.".into(),
            Error::Flash(e) => format!("Installing failed: {e}."),
            Error::Io(e) => format!("File error: {e}."),
        }
    }

    /// One suggested action.
    pub fn action(&self) -> String {
        match self {
            Error::Offline => "Check your internet connection, or use \"Choose file...\" to install from a downloaded release.".into(),
            Error::Http(403) | Error::Http(429) => "Wait a few minutes and start again, or use \"Choose file...\".".into(),
            Error::Http(_) | Error::Network(_) => "Try again in a moment.".into(),
            Error::BadManifest(_) => "Pick another version.".into(),
            Error::HashMismatch(_) | Error::BadFile(_) => "Press Install again to download it anew.".into(),
            Error::NoDevice => {
                if cfg!(windows) {
                    "Connect the M5Paper with a USB cable. If it still does not appear, install the CH9102 USB driver (https://www.wch-ic.com/downloads/CH343SER_EXE.html).".into()
                } else {
                    "Connect the M5Paper with a USB cable (use a data cable, not a charge-only one).".into()
                }
            }
            Error::PortBusy(_) => "Close other programs that may use the port (serial monitors, Arduino, PlatformIO), then try again.".into(),
            Error::NoAnswer(_) => "Unplug the USB cable, plug it in again (or try another cable), then press Install.".into(),
            Error::Flash(_) | Error::Io(_) => "Unplug and replug the USB cable and try again.".into(),
        }
    }

    /// Classify a flasher/serial error text.
    pub fn from_flash_text(port: &str, text: &str) -> Error {
        let t = text.to_lowercase();
        if t.contains("permission denied") || t.contains("busy") || t.contains("access is denied") || t.contains("failed to open") || t.contains("no such file") {
            Error::PortBusy(port.into())
        } else if t.contains("timeout") || t.contains("timed out") || t.contains("sync") || t.contains("failed to connect") || t.contains("invalid header") {
            Error::NoAnswer(text.into())
        } else {
            Error::Flash(text.into())
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} {}", self.message(), self.action())
    }
}

impl std::error::Error for Error {}

impl From<std::io::Error> for Error {
    fn from(e: std::io::Error) -> Self {
        Error::Io(e.to_string())
    }
}

impl From<ureq::Error> for Error {
    fn from(e: ureq::Error) -> Self {
        match e {
            ureq::Error::StatusCode(c) => Error::Http(c),
            ureq::Error::Io(_) | ureq::Error::HostNotFound | ureq::Error::ConnectionFailed | ureq::Error::Timeout(_) => Error::Offline,
            other => Error::Network(other.to_string()),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn classifies_flash_errors() {
        assert!(matches!(Error::from_flash_text("p", "Permission denied"), Error::PortBusy(_)));
        assert!(matches!(Error::from_flash_text("p", "Failed to connect to ESP32: timeout"), Error::NoAnswer(_)));
        assert!(matches!(Error::from_flash_text("p", "weird"), Error::Flash(_)));
    }

    #[test]
    fn every_error_has_message_and_action() {
        for e in [Error::Offline, Error::Http(403), Error::Http(500), Error::NoDevice, Error::HashMismatch("a.bin".into())] {
            assert!(!e.message().is_empty() && !e.action().is_empty());
        }
    }
}
