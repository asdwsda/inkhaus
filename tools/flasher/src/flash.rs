//! Writing the verified image to the device with espflash.

use crate::error::Error;
use crate::source::Image;
use espflash::connection::{Connection, ResetAfterOperation, ResetBeforeOperation};
use espflash::flasher::Flasher;
use espflash::image_format::Segment;
use espflash::target::{Chip, ProgressCallbacks};
use serialport::{FlowControl, SerialPort, UsbPortInfo};
use std::borrow::Cow;
use std::time::Duration;

const BAUD: u32 = 460_800;

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Phase {
    Connecting,
    Writing,
    Verifying,
    Restarting,
}

impl Phase {
    pub fn text(self) -> &'static str {
        match self {
            Phase::Connecting => "Connecting",
            Phase::Writing => "Writing",
            Phase::Verifying => "Verifying",
            Phase::Restarting => "Restarting",
        }
    }
}

/// Overall progress: bytes of all segments written so far / total.
struct Progress<'a> {
    total: usize,
    before: usize,
    seg_total: usize,
    report: &'a mut dyn FnMut(Phase, f32),
}

impl Progress<'_> {
    fn frac(&self, cur: usize) -> f32 {
        (((self.before + cur) as f32) / self.total.max(1) as f32).min(1.0)
    }
}

impl ProgressCallbacks for Progress<'_> {
    fn init(&mut self, _addr: u32, total: usize) {
        self.seg_total = total;
        let f = self.frac(0);
        (self.report)(Phase::Writing, f);
    }
    fn update(&mut self, current: usize) {
        let f = self.frac(current);
        (self.report)(Phase::Writing, f);
    }
    fn verifying(&mut self) {
        let f = self.frac(self.seg_total);
        (self.report)(Phase::Verifying, f);
    }
    fn finish(&mut self, _skipped: bool) {
        self.before += self.seg_total;
    }
}

fn pad4(data: &[u8]) -> Cow<'_, [u8]> {
    if data.len() % 4 == 0 {
        Cow::Borrowed(data)
    } else {
        let mut v = data.to_vec();
        v.resize(data.len().div_ceil(4) * 4, 0xFF);
        Cow::Owned(v)
    }
}

/// Flash all segments at their offsets (no erase of other regions, so NVS survives) and reset.
pub fn flash(port_name: &str, image: &Image, report: &mut dyn FnMut(Phase, f32)) -> Result<(), Error> {
    report(Phase::Connecting, 0.0);
    let port = serialport::new(port_name, 115_200)
        .flow_control(FlowControl::None)
        .open_native()
        .map_err(|e| Error::from_flash_text(port_name, &e.to_string()))?;
    let info = UsbPortInfo { vid: 0, pid: 0, serial_number: None, manufacturer: None, product: None };
    // We reset the board ourselves below (the M5Paper needs DTR and RTS asserted together).
    let conn = Connection::new(port, info, ResetAfterOperation::NoReset, ResetBeforeOperation::DefaultReset, 115_200);
    let mut flasher = Flasher::connect(conn, true, true, false, Some(Chip::Esp32), Some(BAUD))
        .map_err(|e| Error::from_flash_text(port_name, &e.to_string()))?;

    let segments: Vec<Segment> = image.segments.iter().map(|(addr, d)| Segment { addr: *addr, data: pad4(d) }).collect();
    let mut progress = Progress { total: image.total_bytes(), before: 0, seg_total: 0, report };
    flasher
        .write_bins_to_flash(&segments, &mut progress)
        .map_err(|e| Error::from_flash_text(port_name, &e.to_string()))?;

    report(Phase::Restarting, 1.0);
    let mut serial = flasher.into_connection().into_serial();
    let _ = serial.write_data_terminal_ready(true);
    let _ = serial.write_request_to_send(true);
    std::thread::sleep(Duration::from_millis(150));
    let _ = serial.write_data_terminal_ready(false);
    let _ = serial.write_request_to_send(false);
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn pads_to_multiple_of_four() {
        assert_eq!(pad4(&[1, 2, 3, 4]).len(), 4);
        assert_eq!(&pad4(&[1, 2, 3])[..], &[1, 2, 3, 0xFF]);
    }

    #[test]
    fn progress_accumulates_over_segments() {
        let mut seen = Vec::new();
        {
            let mut cb = |p: Phase, f: f32| seen.push((p, f));
            let mut pr = Progress { total: 100, before: 0, seg_total: 0, report: &mut cb };
            pr.init(0, 40);
            pr.update(20);
            pr.verifying();
            pr.finish(false);
            pr.init(0x8000, 60);
            pr.update(30);
        }
        assert_eq!(seen[1], (Phase::Writing, 0.2));
        assert_eq!(seen[2].0, Phase::Verifying);
        assert_eq!(seen.last().unwrap().1, 0.7);
    }
}
