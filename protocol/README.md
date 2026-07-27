# D-Bus protocol

The runtime protocol is defined by ADR 0004. The introspection XML and generated
bindings will live in this directory when the IPC implementation is added.

Compatibility rules:

- adding an optional capability is backward-compatible;
- changing a method signature requires a new interface version;
- unknown capabilities are ignored;
- unknown commands, malformed values, and oversized payloads are rejected;
- no implementation may silently downgrade across major versions.
