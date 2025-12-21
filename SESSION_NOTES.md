# Session Notes

## How to keep context
- Capture key steps and decisions here after each session; include TODOs and open questions.
- Commit or stash work (`git status` / `git stash`) so it can be restored later.
- Keep relevant files open in your IDE and note which ones matter.
- For reproducibility, record test commands and their results.

## Current changes snapshot
- Branch: `main` with local mods (ahead/behind origin unknown). Untracked: multiple `.DS_Store` files.
- `platformio.ini`: added Modbus/eModbus dependencies (`eModbus`, `ModBusTCP`).
- `Globals`: new `EXTERNAL_API_URL`, `EXTERNAL_API_INTERVAL_MIN`, `PV_Power_total`; `DEBUG_MODE` default now true.
- `ServerManager`: web UI + settings for external API; periodic `handleExternalApi()` HTTP GET with cooldown.
- `PeripheryManager`/`Apps`/`icons`: fetch PV power via iSolarCloud API (token refresh, HTTPS) and show watts with new icon `icon_27283` in battery app.
- Other: new `DEBUG_PRINT` macro; new icon data in `src/icons.h`.

## Next steps ideas
- Decide on proper tokens/URLs and secure handling for the PV API; consider cert pinning.
- Clean up `.DS_Store` files if unneeded.
- Add tests or manual checks for the new external API polling and PV display.
