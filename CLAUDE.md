# Synergy — collaboration notes

## Code style

- Default to **no comments**. Only add one when the WHY is non-obvious — hidden constraints, subtle invariants, workarounds for specific bugs, behavior that would surprise a reader. Never add comments that explain WHAT or HOW (well-named identifiers do that).
- Don't reference the current task, fix, ticket, or CVE in comments. That belongs in the commit message.

## Working with commits

- **Never commit, amend, or rewrite history.** The user reviews staged changes and commits themselves. No `git commit`, no `git commit --amend`, no `git rebase`, no force-pushes.
- When cherry-picking, use `git cherry-pick --no-commit` so changes land in the index/working tree without producing a commit. Halt after conflict resolution and let the user commit.

## Backporting from upstream Deskflow

- Synergy is a downstream fork of [deskflow/deskflow](https://github.com/deskflow/deskflow). The `deskflow` git remote should already be configured.
- Branding is driven by `DESKFLOW_APP_ID = "synergy"` in `ext/synergy-extra/cmake/Extra.cmake` (the `synergy-extra` overlay). Most of `src/` is shared verbatim with deskflow upstream.
- Synergy doesn't track deskflow master continuously. Synergy is the more mature, stabilized branch: it cherry-picks critical updates (security fixes, important bugfixes) from Deskflow and occasionally re-forks from upstream as a beta. Deskflow master is the rolling dev branch with in-progress refactors; Synergy is the curated subset.
- Practical consequence: when porting from a deskflow PR, expect direct `git cherry-pick` to fail. Files may live at different paths than upstream (e.g. upstream may have moved `DaemonIpcServer.cpp` `win32/` → `ipc/` or `DaemonApp.cpp` `lib/` → `apps/` while Synergy still has the original location), and surrounding refactors may not be present. Plan to port hunks manually into the Synergy file locations.
- **Backport multi-commit PRs one commit at a time.** Apply, surface conflicts, resolve together, wait for the user to review/commit, then move to the next.
