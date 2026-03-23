# Claude Code — Project Rules

These rules apply to every Claude Code session in this repository.

## Git Workflow

- **Never commit directly to `main`.** Always create a feature branch first.
- **Never push directly to `main`.**
- All changes must go through a pull request.
- Feature branch naming convention: `feat/`, `fix/`, `chore/`, `refactor/`, `docs/`
- Use [Conventional Commits](https://www.conventionalcommits.org/) for commit messages:
  - `feat:` new feature
  - `fix:` bug fix
  - `chore:` housekeeping / tooling (no production code change)
  - `refactor:` code restructure with no behavior change
  - `docs:` documentation only
  - `test:` test additions or changes

## Project Structure

This is a two-project monorepo:

| Directory | Stack | Purpose |
|---|---|---|
| `firmware/` | C++ / PlatformIO / ESP32 | Real-time motion controller — REST API + WebSocket only |
| `ui/` | React + Vite + TypeScript + TanStack Query | iPad Pro PWA — connects to firmware or simulator |
| `sim/` | Node.js / Express / ws | Mock ESP32 for UI development without hardware |
| `docs/` | Markdown | Architecture, security plan, API docs |

## Development Environment

### Simulator
- Start the sim: `cd sim && npm run dev`
- The sim token is pinned via `sim/.env` (gitignored — create locally, see below)
- Token persists across restarts when `SIM_TOKEN` is set in `sim/.env`

### UI
- Start the UI: `cd ui && npm run dev`
- In dev mode the UI auto-pairs using `VITE_DEV_TOKEN` from `ui/.env.local` (gitignored)
- Create `ui/.env.local` with: `VITE_DEV_TOKEN=<token from sim output>`
- Without `VITE_DEV_TOKEN` the pairing screen appears — enter the token manually once

### Local env files (gitignored, create manually)
```
sim/.env          →  SIM_TOKEN=<64-char hex>
ui/.env.local     →  VITE_DEV_TOKEN=<same 64-char hex>
```

## Security Notes

- `firmware/include/credentials.h` is gitignored — never commit it
- `sim/.env` is gitignored — never commit it
- `ui/.env.local` is gitignored — never commit it
- See `docs/SECURITY.md` for the full security posture

## Firmware Build

```bash
cd firmware
pio run                  # build
pio run -t upload        # flash to ESP32
pio device monitor       # serial monitor (token printed on first boot)
```

## UI Build

```bash
cd ui
npm install
npm run dev              # dev server at http://localhost:5173
npm run build            # production bundle
```
