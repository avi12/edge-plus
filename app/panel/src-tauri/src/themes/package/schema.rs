//! Reads like zod: a type that derives `Deserialize` is the schema, and
//! `parse::<Manifest>(bytes)` is `Manifest.parse(data)`. A value that checks
//! itself (`Color`, `Version`) implements `Deserialize` once and every schema
//! that has one inherits it. A refusal names where it is, as zod's issue path
//! does: `theme.colors.frame: needs 3 or 4 numbers`.

use serde::Deserializer;
use serde::de::{DeserializeOwned, Error};
use serde_json::Deserializer as JsonDeserializer;

/// Why a JSON text is not the shape the schema asks for.
#[derive(Debug, PartialEq, Eq)]
pub enum SchemaError {
    /// The text is not JSON; the line is where the parser gave up.
    Syntax { line: usize },
    /// The JSON is not the shape: where, and what was wrong there.
    Shape { path: String, reason: String },
}

impl SchemaError {
    /// `path: reason`, or the reason alone for a problem with the whole value.
    #[must_use]
    pub fn sentence(&self) -> String {
        match self {
            Self::Syntax { line } => format!("not valid JSON (line {line})"),
            Self::Shape { path, reason } if path == "." => reason.clone(),
            Self::Shape { path, reason } => format!("{path}: {reason}"),
        }
    }
}

const POSITION_MARKER: &str = " at line ";

/// `schema.parse(bytes)`: the value, or the first thing wrong with it.
pub fn parse<T: DeserializeOwned>(bytes: &[u8]) -> Result<T, SchemaError> {
    let mut deserializer = JsonDeserializer::from_slice(bytes);
    serde_path_to_error::deserialize(&mut deserializer).map_err(|error| {
        let path = error.path().to_string();
        let inner = error.into_inner();
        if inner.is_data() {
            // serde_json appends where it was in the text; a data refusal is
            // about the shape, so the position is cut.
            let message = inner.to_string();
            let reason = message
                .split(POSITION_MARKER)
                .next()
                .unwrap_or(&message)
                .to_owned();
            return SchemaError::Shape { path, reason };
        }
        SchemaError::Syntax { line: inner.line() }
    })
}

/// `.refine(...)`: a refusal with a sentence of its own, for a `Deserialize`
/// that checks what the type cannot.
pub fn refuse<'de, D: Deserializer<'de>>(reason: impl std::fmt::Display) -> D::Error {
    D::Error::custom(reason)
}
