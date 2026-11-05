//! Windows' own stores and services: the registry, MSIX packages, toasts and
//! CNG keys.

pub mod package_identity;
pub mod packages;
pub mod registry;
pub mod rsa;
pub mod startup_task;
pub mod toast;
