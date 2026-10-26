//! A theme as a file: its manifest, the CRX3 container and protobuf header it
//! is packed in, and the extension id derived from its key.

pub mod crx;
pub mod extension_id;
pub mod protobuf;
pub mod theme_manifest;
pub mod theme_packer;
