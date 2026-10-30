//! GitHub Releases list: parsing and selection.

use crate::error::Error;
use serde::Deserialize;
use std::time::Duration;

pub const DEFAULT_REPO: &str = "asdwsda/inkhaus";

#[derive(Debug, Clone, Deserialize, PartialEq)]
pub struct Asset {
    pub name: String,
    pub browser_download_url: String,
}

#[derive(Debug, Clone, Deserialize, PartialEq)]
pub struct Release {
    pub tag_name: String,
    #[serde(default)]
    pub draft: bool,
    #[serde(default)]
    pub prerelease: bool,
    #[serde(default)]
    pub assets: Vec<Asset>,
}

impl Release {
    pub fn asset_url(&self, name: &str) -> Option<&str> {
        self.assets.iter().find(|a| a.name == name).map(|a| a.browser_download_url.as_str())
    }
}

/// Parse the API answer; drafts and releases without manifest.json are dropped.
/// GitHub returns newest first, and that order is kept.
pub fn parse(json: &str) -> Result<Vec<Release>, Error> {
    let all: Vec<Release> = serde_json::from_str(json).map_err(|e| Error::Network(format!("unexpected answer from GitHub: {e}")))?;
    Ok(all.into_iter().filter(|r| !r.draft && r.asset_url("manifest.json").is_some()).collect())
}

/// Releases to show in the picker.
pub fn visible(all: &[Release], show_prereleases: bool) -> Vec<&Release> {
    all.iter().filter(|r| show_prereleases || !r.prerelease).collect()
}

/// Index (within `visible`) of the preselected entry: the newest stable one, else the first.
pub fn default_index(list: &[&Release]) -> usize {
    list.iter().position(|r| !r.prerelease).unwrap_or(0)
}

pub fn agent() -> ureq::Agent {
    ureq::Agent::config_builder()
        .timeout_global(Some(Duration::from_secs(120)))
        .timeout_connect(Some(Duration::from_secs(10)))
        .build()
        .into()
}

/// Fetch the release list once (60 requests/hour unauthenticated).
pub fn fetch(repo: &str) -> Result<Vec<Release>, Error> {
    let url = format!("https://api.github.com/repos/{repo}/releases?per_page=30");
    let mut resp = agent()
        .get(&url)
        .header("User-Agent", "m5paper-flasher")
        .header("Accept", "application/vnd.github+json")
        .call()?;
    let text = resp.body_mut().read_to_string()?;
    parse(&text)
}

#[cfg(test)]
mod tests {
    use super::*;

    const FIXTURE: &str = include_str!("../tests/fixtures/releases.json");

    #[test]
    fn drops_drafts_and_releases_without_manifest() {
        let r = parse(FIXTURE).unwrap();
        let tags: Vec<_> = r.iter().map(|r| r.tag_name.as_str()).collect();
        assert_eq!(tags, ["v1.2.0-rc1", "v1.0.0"]);
    }

    #[test]
    fn stable_is_preselected_and_prereleases_toggle() {
        let r = parse(FIXTURE).unwrap();
        let stable = visible(&r, false);
        assert_eq!(stable.len(), 1);
        assert_eq!(stable[0].tag_name, "v1.0.0");
        let all = visible(&r, true);
        assert_eq!(all.len(), 2);
        assert_eq!(all[default_index(&all)].tag_name, "v1.0.0");
    }

    #[test]
    fn finds_asset_urls() {
        let r = parse(FIXTURE).unwrap();
        assert!(r[1].asset_url("firmware.bin").unwrap().ends_with("/v1.0.0/firmware.bin"));
        assert!(r[1].asset_url("nope").is_none());
    }

    #[test]
    fn empty_and_garbage() {
        assert!(parse("[]").unwrap().is_empty());
        assert!(parse("{\"message\":\"rate limit\"}").is_err());
    }
}
