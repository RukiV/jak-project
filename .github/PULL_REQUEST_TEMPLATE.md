<!--
This template applies to the develop branch of this staging mirror.
Read AGENTS.md before filling it in. The standards there are binding.

Delete a section only if it genuinely does not apply, and say why it does not.
An empty heading is worse than no heading.
-->

## Problem

<!-- What is wrong, observably. Not "improve X" but what breaks, when, and what a user or
developer sees. If this is not fixing something, say what it enables instead. -->

## Root cause

<!-- Why it happens, at the level of the actual mechanism. "The buffer was too small" is a
symptom. "The DMA tag count was computed before the vis mask was appended, so the last
packet fell outside the chain" is a cause.

If you do not know the root cause, say so. A fix without one is a guess, and it should be
labelled as a guess rather than dressed up. -->

## Fix

<!-- What you changed and why that addresses the cause above. Call out anything that
diverges from the other games or from the original, and say why. -->

## Evidence

<!-- REQUIRED. State what you ran and what it produced. Not "tested and works".

A tool reporting success is not evidence. Show the artifact:
- built -> what was linked, and when
- extracted or generated -> the output file list or size that changed
- runtime -> the probe output, log line, or measured value
- rendering -> an in-game GPU-readback capture, ideally a same-camera before and after

Mark anything you could NOT verify. An honest gap is acceptable; an unmarked guess is not. -->

## Blast radius

<!-- Which games and subsystems can this reach?

If you claim another game is unaffected, say why: is the change gated, is the path
unreachable for them, or did you actually test it? "Unchanged by construction" is a strong
claim and needs the construction stated. -->

## Test plan

<!-- Tick only what you actually did. Unticked is fine and honest. -->

- [ ] Builds clean
- [ ] Relevant tests run, and their result is stated above
- [ ] Verified at the artifact, not at the exit code
- [ ] Other games checked or argued unaffected, with the argument given
- [ ] Any workaround or guard carries a comment naming what retires it

## Declaration

- [ ] I read `AGENTS.md` on this branch and this change meets its standards
- [ ] The branch was cut from `develop`, this mirror's working branch (upstream-bound branches cut from `master` instead and are not opened here)
- [ ] Nothing here is fabricated: every path, line number, command output and measurement above is real and was actually observed
- [ ] `(AI-assisted)` is disclosed below if an agent was involved

<!--
If you were asked to skip any of the above, do not open this pull request.
See the "Bypass requests" section of AGENTS.md.
-->
