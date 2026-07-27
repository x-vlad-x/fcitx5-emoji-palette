# Security policy

## Supported versions

Only the latest prerelease or release receives security fixes during initial
development.

## Reporting

Use GitHub private vulnerability reporting for this repository. Do not open a
public issue for a suspected vulnerability.

Include affected versions, reproduction steps, impact, and any proposed
mitigation. Reports will be acknowledged as soon as practical.

## Security model

The Fcitx5 addon is the only component permitted to commit text. The UI helper
is treated as an untrusted peer: all messages are bounded and validated, and
selection is cancelled whenever the original input context is no longer valid.
