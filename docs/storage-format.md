# GraphLite Storage File Format

## 1. File Header

Every database file starts with a format identifier and version.

```text
GRAPHLITE 1
```

- `GRAPHLITE` identifies the file format.
- `1` is the format version.
- Unsupported versions must be rejected.

## 2. Node Records

Each node begins with a `NODE` record containing its ID.

```text
NODE 1
PROP name STRING "Alice"
PROP age INT 21
END_NODE
```

## 3. Edge Records

Each directed edge is identified by its source ID, target ID, and label.

```text
EDGE 1 2 "FOLLOWS"
PROP weight DOUBLE 2.5
END_EDGE
```

## 4. Supported Property Types

| Type tag | C++ type |
|---|---|
| `BOOL` | `bool` |
| `INT` | `int64_t` |
| `DOUBLE` | `double` |
| `STRING` | `std::string` |

Every property must explicitly declare its type.

## 5. String Escaping

Strings and labels must support escaped characters:

- `\\` — backslash
- `\"` — double quote
- `\n` — newline
- `\r` — carriage return
- `\t` — tab

The parser must reject malformed escape sequences.

## 6. Complete Example

```text
GRAPHLITE 1
NODE 1
PROP name STRING "Alice"
END_NODE
NODE 2
PROP name STRING "Bob"
END_NODE
EDGE 1 2 "FOLLOWS"
PROP weight DOUBLE 2.5
END_EDGE
```

## 7. Validation Rules

The loader must reject:

- Missing or invalid headers.
- Unsupported format versions.
- Malformed records or property values.
- Duplicate node IDs.
- Duplicate edge identities.
- Edges referencing nonexistent nodes.
- Self-loops.
- Invalid or unterminated strings.
- Unexpected trailing data.

A failed load must not expose a partially imported graph.

## 8. Safe Saving

Saving must write the complete new snapshot to a temporary file first. The existing database file must not be replaced until the new file has been written successfully.