# Contributing to Muninn

Thanks for helping build Muninn. This repo follows **Git Flow** and a few firm conventions.

## Branching model — Git Flow

| Branch | Purpose | Base | Merges into |
|--------|---------|------|-------------|
| `main` | Production-ready, tagged releases only | — | — |
| `develop` | Integration branch; default branch | `main` | — |
| `feature/*` | New work | `develop` | `develop` |
| `release/*` | Release stabilization | `develop` | `main` **and** `develop` |
| `hotfix/*` | Urgent production fixes | `main` | `main` **and** `develop` |

- Branch **off `develop`** for features: `git switch -c feature/usb-uac-source develop`.
- Open PRs **into `develop`** (or into `main` for `release/*` and `hotfix/*`).
- `main` and `develop` are protected — no direct pushes; land everything via PR.

## Merge policy

- **Always merge with a merge commit** — `gh pr merge --merge`. Never squash-merge or rebase-merge.
- **Never merge a PR with red or pending CI.** Wait for every required check to pass on the PR's
  current head commit. A pre-existing, flaky, or seemingly unrelated failure is still a blocker —
  fix it or stop and ask, but do not waive.

## Commit style

- Present-tense, imperative subject (`Add USB-CDC transport framing`).
- Reference issues/PRs with full links where relevant.
- Keep firmware and listener changes in separate commits when practical.

## Before you push

**Firmware** (`firmware/`):
```bash
pio run -e esp32-s3-usb     # builds
pio test -e native          # host unit tests pass
```

**Listener** (`listener/`):
```bash
ruff check . && ruff format --check .
pytest
```

CI runs the same checks on every PR into `main`/`develop`.

## Documentation is Markdown-canonical

The `.md` file is the source of truth for any document. PDFs, `.docx`, etc. are rendered artifacts —
never hand-edit them; edit the Markdown and re-render.

## Code layout conventions

- Firmware front-ends implement the `AudioSource` interface (`firmware/src/audio/audio_source.h`) so
  the USB and Bluetooth paths stay swappable. Transports implement the sink interface in
  `firmware/src/transport/`.
- Keep the [wire protocol](docs/protocol.md) authoritative: the firmware framing and the Python
  `muninn_listener` decoder must agree byte-for-byte. Change both together, and bump the protocol
  version.
