# 0011 — Documentation update: Safe vs Active sandwiching

**What to build:** Update project documentation to reflect the new Active-vs-Safe sandwiching distinction and the CLI surface, referencing ADR 0002 per the documentation lifecycle. Ensures the docs match runtime behavior so future readers understand why Active mode matters.

**Blocked by:** 0010

**Status:** ready-for-agent

- [ ] `DEFINITION.md` — clarify the Sandwiching entry (Safe vs Active distinction)
- [ ] `CONTEXT.md` — Phase 2 sandwiching description mentions both modes and the Active default
- [ ] `README.md` — document `--sandwich-mode` CLI and update sample output
- [ ] `docs/adr/0002-active-sandwiching-mode.md` exists and is appropriately referenced from `CONTEXT.md` / `DEFINITION.md` (not merely mentioned in this ticket)
- [ ] Docs accurate against the verified runtime behavior (Active default, `--no-sandwich` precedence)
