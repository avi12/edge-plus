//! Installing MSIX packages for the current user (no elevation): how the core
//! puts the Windows App runtime the window needs on a machine that lacks it.

use std::io;
use std::path::Path;

use windows::Foundation::Uri;
use windows::Management::Deployment::{DeploymentOptions, PackageManager};
use windows::core::HSTRING;

/// Each package in turn, a framework before the packages that depend on it.
/// One already installed at this version or newer is a success.
pub fn install_packages(packages: &[&Path]) -> io::Result<()> {
    let manager = PackageManager::new().map_err(io::Error::other)?;
    for package in packages {
        let uri = Uri::CreateUri(&HSTRING::from(package.as_os_str())).map_err(io::Error::other)?;
        manager
            .AddPackageAsync(&uri, None, DeploymentOptions::None)
            .and_then(|operation| operation.get())
            .map_err(io::Error::other)?;
    }
    Ok(())
}
