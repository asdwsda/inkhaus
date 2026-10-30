//! Finding the M5Paper's serial port.

use serialport::SerialPortType;

/// USB ids that identify an M5Paper: CH9102 (1a86:55d4, later units) and CP2104 (10c4:ea60, early units).
const KNOWN: [(u16, u16); 2] = [(0x1a86, 0x55d4), (0x10c4, 0xea60)];

#[derive(Debug, Clone, PartialEq)]
pub struct PortEntry {
    pub name: String,
    pub is_m5paper: bool,
    pub usb: bool,
}

pub fn is_m5paper(vid: u16, pid: u16) -> bool {
    KNOWN.contains(&(vid, pid))
}

pub fn classify(name: &str, t: &SerialPortType) -> PortEntry {
    match t {
        SerialPortType::UsbPort(u) => PortEntry { name: name.into(), is_m5paper: is_m5paper(u.vid, u.pid), usb: true },
        _ => PortEntry { name: name.into(), is_m5paper: false, usb: false },
    }
}

/// All USB serial ports (other port kinds, such as legacy ttyS*, are skipped), M5Paper candidates first.
pub fn scan() -> Vec<PortEntry> {
    let mut v: Vec<PortEntry> = serialport::available_ports()
        .unwrap_or_default()
        .iter()
        .map(|p| classify(&p.port_name, &p.port_type))
        .filter(|p| p.usb)
        .collect();
    // On macOS every device shows up twice (cu.* and tty.*); keep the cu.* one.
    let names: Vec<String> = v.iter().map(|p| p.name.clone()).collect();
    v.retain(|p| !p.name.contains("/tty.") || !names.contains(&p.name.replace("/tty.", "/cu.")));
    v.sort_by_key(|p| (!p.is_m5paper, p.name.clone()));
    v
}

pub fn candidates(all: &[PortEntry]) -> Vec<&PortEntry> {
    all.iter().filter(|p| p.is_m5paper).collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use serialport::UsbPortInfo;

    fn usb(vid: u16, pid: u16) -> SerialPortType {
        SerialPortType::UsbPort(UsbPortInfo { vid, pid, serial_number: None, manufacturer: None, product: None })
    }

    #[test]
    fn recognises_m5paper_ids() {
        assert!(classify("COM3", &usb(0x1a86, 0x55d4)).is_m5paper);
        assert!(classify("/dev/ttyUSB0", &usb(0x10c4, 0xea60)).is_m5paper);
        assert!(!classify("/dev/ttyUSB1", &usb(0x1234, 0x0001)).is_m5paper);
        let p = classify("/dev/ttyS0", &SerialPortType::Unknown);
        assert!(!p.usb && !p.is_m5paper);
    }

    #[test]
    fn candidates_filter() {
        let all = vec![classify("a", &usb(0x1a86, 0x55d4)), classify("b", &usb(1, 2))];
        assert_eq!(candidates(&all).len(), 1);
    }
}
