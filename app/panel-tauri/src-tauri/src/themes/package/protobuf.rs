//! The slice of the protobuf wire format a CRX3 header uses: varints and
//! length-delimited fields.

use crate::themes::package::theme_manifest::{ThemeFileResult, invalid};
use crate::ui::wording;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u64)]
enum WireType {
    LengthDelimited = 2,
}

const WIRE_TYPE_BITS: u32 = 3;
const WIRE_TYPE_MASK: u64 = 0x7;
const VARINT_PAYLOAD_BITS: u32 = 7;
const VARINT_PAYLOAD_MASK: u8 = 0x7f;
const VARINT_CONTINUES: u8 = 0x80;
const MAX_VARINT_SHIFT: u32 = 63;

/// A closed set of a message's field numbers (an enum per message).
pub trait FieldNumber: Copy {
    fn number(self) -> u64;
}

/// A length-delimited field: its payload is `data[start..start + length]`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Field {
    pub number: u64,
    pub start: usize,
    pub length: usize,
}

impl Field {
    #[must_use]
    pub fn is_number(&self, expected: impl FieldNumber) -> bool {
        self.number == expected.number()
    }

    #[must_use]
    pub fn payload<'a>(&self, data: &'a [u8]) -> &'a [u8] {
        &data[self.start..self.start + self.length]
    }
}

/// The value and the offset after it.
pub fn read_varint(data: &[u8], offset: usize) -> ThemeFileResult<(u64, usize)> {
    let mut value = 0u64;
    let mut shift = 0u32;
    let mut offset = offset;
    loop {
        let piece = data.get(offset).copied();
        let Some(piece) = piece.filter(|_| shift <= MAX_VARINT_SHIFT) else {
            return Err(invalid(wording::TRUNCATED_VARINT));
        };
        offset += 1;
        value |= u64::from(piece & VARINT_PAYLOAD_MASK) << shift;
        let is_last = piece & VARINT_CONTINUES == 0;
        if is_last {
            return Ok((value, offset));
        }
        shift += VARINT_PAYLOAD_BITS;
    }
}

/// Any wire type but length-delimited is refused: nothing a CRX header carries
/// uses one.
pub fn read_fields(data: &[u8], start: usize, end: usize) -> ThemeFileResult<Vec<Field>> {
    let mut fields = Vec::new();
    let mut offset = start;
    while offset < end {
        let (tag, after_tag) = read_varint(data, offset)?;
        let is_length_delimited = tag & WIRE_TYPE_MASK == WireType::LengthDelimited as u64;
        if !is_length_delimited {
            return Err(invalid(wording::UNEXPECTED_CRX_HEADER_FIELD));
        }
        let (length, payload_start) = read_varint(data, after_tag)?;
        let room = end.saturating_sub(payload_start);
        let length = usize::try_from(length)
            .ok()
            .filter(|&length| length <= room)
            .ok_or_else(|| invalid(wording::TRUNCATED_CRX_HEADER))?;
        fields.push(Field {
            number: tag >> WIRE_TYPE_BITS,
            start: payload_start,
            length,
        });
        offset = payload_start + length;
    }
    Ok(fields)
}

#[must_use]
pub fn varint(value: u64) -> Vec<u8> {
    let mut bytes = Vec::new();
    let mut value = value;
    loop {
        let low = value.to_le_bytes()[0] & VARINT_PAYLOAD_MASK;
        value >>= VARINT_PAYLOAD_BITS;
        if value == 0 {
            bytes.push(low);
            return bytes;
        }
        bytes.push(low | VARINT_CONTINUES);
    }
}

#[must_use]
pub fn length_delimited_field(number: impl FieldNumber, payload: &[u8]) -> Vec<u8> {
    let mut field = varint((number.number() << WIRE_TYPE_BITS) | WireType::LengthDelimited as u64);
    field.extend_from_slice(&varint(payload.len() as u64));
    field.extend_from_slice(payload);
    field
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Clone, Copy)]
    struct Number(u64);

    impl FieldNumber for Number {
        fn number(self) -> u64 {
            self.0
        }
    }

    #[test]
    fn protobuf_fields_round_trip() {
        assert_eq!(vec![0x90, 0x4e], varint(10_000));
        let mut data = length_delimited_field(Number(10_000), b"abc");
        data.extend_from_slice(&length_delimited_field(Number(1), b""));
        let fields = read_fields(&data, 0, data.len()).expect("fields");
        assert_eq!(2, fields.len());
        assert!(fields[0].is_number(Number(10_000)));
        assert_eq!(b"abc", fields[0].payload(&data));
        assert!(fields[1].is_number(Number(1)) && fields[1].length == 0);
    }

    #[test]
    fn protobuf_refuses_truncated_or_foreign_fields() {
        assert!(read_varint(&[0x80], 0).is_err());
        let overlong = length_delimited_field(Number(1), b"abc");
        assert!(read_fields(&overlong, 0, overlong.len() - 1).is_err());
        // Field 1 as a varint (wire type 0).
        assert!(read_fields(&[0x08, 0x01], 0, 2).is_err());
    }
}
