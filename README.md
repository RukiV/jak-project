> [!WARNING]
> **This is not the official OpenGOAL repository.** It is a personal staging mirror.
> The official project lives at **[open-goal/jak-project](https://github.com/open-goal/jak-project)**,
> and that is where issues, releases and support belong.
>
> Work here is in progress and may be wrong, unfinished, or specific to one machine.

# Jak X bring-up

This fork exists to bring up **Jak X: Combat Racing**. That is its purpose, and
everything else here exists to serve it.

Jak X currently boots, loads levels, and renders its world and sky: tfrag, tie and shrub
geometry under a mood-driven lighting chain, with a full sky (dome, stars, sun and moon,
haze, clouds and fog) and an advancing time-of-day clock. **Actors do not render yet**,
which is the next frontier rather than a small gap.

Upstream already carries Jak X support, including a large bulk landing in
[#4112](https://github.com/open-goal/jak-project/pull/4112), so this builds on existing
work rather than starting something new. Fixes found along the way are developed and
reviewed here, then offered upstream on their own merits.

| Where the work is tracked | |
| --- | --- |
| Tracking issue | #18 |
| Staging pull request, full milestone history | #19 |
| Plan for submitting upstream | #36 |
| Operational setup: checkouts, worktrees, booting Jak X, REPL verification | [`docs/jakx-bringup-quickstart.md`](docs/jakx-bringup-quickstart.md) |

## How this mirror is arranged

`master` is kept byte-identical to upstream, so a branch cut from it produces a clean
upstream diff. Nothing here is authoritative.

| Branch | Role |
| --- | --- |
| `master` | Mirrors upstream. Never merged into |
| `develop` | Integration. Pull requests here target this branch and are genuinely merged |
| `fix/*` | One fix each, always cut from `master` so they stay upstream-submittable |
| `jakx-split/*` | Read-only markers at verified checkpoints of the Jak X bring-up |

Branches are cut from `master`, never from `develop`. A branch cut from `develop` carries
every other merged fix, so its upstream diff would include unrelated work.

### How a fix moves

1. Branch from `master`
2. Pull request into `develop`, reviewed and merged
3. Offered upstream from the same branch, if and when it is ready
4. Labels track the upstream half: `state/staged`, then `state/submitted`, then
   `state/merged-upstream`

The pull request here answers "is this accepted into my tree". Whether it reached upstream
is tracked by the `state/*` labels, because a forge pull request cannot know that.

## Building, and everything else about OpenGOAL

Read the [upstream README](https://github.com/open-goal/jak-project#readme). Setup,
supported platforms, build instructions and the technical overview all live there and are
maintained there.

This file used to carry a verbatim copy of it. That copy went stale the moment upstream
touched theirs, and a stale copy of a build guide is worse than a link to a current one.
For the parts specific to *this* fork, see the quickstart linked above.

## Contributing

Please contribute to [open-goal/jak-project](https://github.com/open-goal/jak-project)
instead. Pull requests opened here will not reach the upstream project.

Agents working in this repository must read [`AGENTS.md`](AGENTS.md) first.
