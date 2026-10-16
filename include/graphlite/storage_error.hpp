
#pragma once

namespace graphlite {

enum class StorageError {
    None,
    FileOpenError,
    FileWriteError,
    FileReadError,
    InvalidFormat,
    ParseError,
    DuplicateNode,
    DuplicateEdge,
    MissingEndpoint,
    SelfLoop,
    UnsupportedVersion,
    NotReady,
    MissingEdge,

    // New validation and property errors.
    InvalidName,
    MissingProperty
};

} // namespace graphlite
