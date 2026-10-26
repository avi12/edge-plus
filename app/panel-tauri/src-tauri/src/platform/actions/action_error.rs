//! Why an action the window asked for failed. Only a `User` error carries a
//! sentence written for the user; anything else is logged, and the window says
//! `wording::ACTION_FAILED` instead of a raw message.

use std::fmt;
use std::io;

#[derive(Debug)]
pub enum ActionError {
    /// Worded for the user (wording.rs); shown as it is.
    User(String),
    /// For the log only.
    Internal(String),
}

pub type ActionResult<T> = Result<T, ActionError>;

impl fmt::Display for ActionError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::User(message) | Self::Internal(message) => formatter.write_str(message),
        }
    }
}

impl From<io::Error> for ActionError {
    fn from(error: io::Error) -> Self {
        Self::Internal(error.to_string())
    }
}
