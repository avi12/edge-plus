//! Kernel objects and primitives: owned handles, named events and mutexes,
//! UTF-16 strings, local time and file versions.

pub(crate) mod clock;
pub(crate) mod handle;
pub(crate) mod sync;
pub(crate) mod version;
pub(crate) mod wide;
