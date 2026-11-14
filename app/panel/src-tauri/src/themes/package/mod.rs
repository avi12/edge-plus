//! A theme as a file: its manifest, the CRX3 container and protobuf header it
//! is packed in, and the extension id derived from its key.

pub(crate) mod crx;
pub(crate) mod edge_loader;
pub(crate) mod extension_id;
pub(crate) mod manifest;
pub(crate) mod protobuf;
pub(crate) mod schema;
pub(crate) mod theme_manifest;
pub(crate) mod theme_packer;
