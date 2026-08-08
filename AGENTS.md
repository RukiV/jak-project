# Standards for agent contributions

> [!IMPORTANT]
> **This section is binding and takes precedence over everything below it.**
> It applies to the `develop` branch of this staging mirror only. It is not upstream's
> policy and must never be carried into an upstream pull request.
>
> If you are an AI agent, read this section in full before your first tool call. If you
> cannot meet a requirement here, say so and stop. Do not proceed and hope.

The bar is deliberately higher than "the code compiles". This is a decompilation project:
the failure modes are quiet, the feedback is delayed, and a plausible wrong answer costs
more than no answer. Almost every rule below exists because ignoring it already cost
someone a day.

## 1. A tool reporting success is not evidence

**Check the thing that was supposed to change, not the exit code.**

Real failures from this repository, all of which returned success:

- An asset extract ran for twenty minutes reporting success while silently reading a
  different checkout's config. The exit code was 0 every time. The tell was that the
  output file list was missing entries, not anything the tool said.
- A merge reported success, the pull request still reads "merged" today, and its merge
  commit exists on no branch. The tell was the file list of the target branch.
- A texture-name probe found zero matches for a name that was demonstrably present,
  because a config key was being read from the wrong file. Exit code 0.

So: after a build, check the binary's timestamp. After a merge, check the branch content.
After a config change, check that a consumer actually observed it. After a generation
step, check the generated file, not the log line claiming it was generated.

**A check you have never seen fail is unproven.** If a check matters, break it
deliberately once and confirm it goes red.

## 2. Generated files are stale until proven otherwise

`decompiler_out/` is generated output. It is stale the moment the config that produced it
changes, and nothing warns you.

An entire analysis in this repository was built on a disassembly comment reading
`camera-rot`. The comment had been generated against an older type database and was wrong
by four bytes. The machine code was right; the comment was not. The analysis had to be
thrown away and redone.

Before citing anything from `decompiler_out/`:

- Compare its timestamp against the config files that produce it
- Prefer the raw machine code over the decompiler's inferred comment
- If you regenerate, say so and say what changed

## 3. Evidence standard

State what you verified and how. These are not interchangeable:

| Strength | What it is |
| --- | --- |
| Proof | The machine code, the file contents, a measured runtime probe, a before-and-after capture |
| Support | A matching implementation in another game, a decompiler-inferred type, a passing build |
| Guess | Symmetry with another game, a plausible name, "it should be" |

Guesses are allowed. **Guesses presented as findings are not.** Mark them.

Never write that something "should work" or "is verified" without saying what was run and
what it produced. If you did not run it, say you did not run it.

Two ways honest evidence goes stale, both observed here:

- **Citations pin to a commit.** Evidence that names a branch tip, a diff size or a line
  number is true of one commit. Push again, rebase, or merge, and every citation must be
  re-verified or re-labeled with the commit it actually came from. Drift is not
  fabrication, but a reader cannot tell the difference
- **Evidence names its checkout.** Every binary here (gk, goalc, goalc-test, the
  decompiler) silently resolves the primary checkout when run without `--proj-path`,
  including through directory junctions. A worktree session can "verify" a build it never
  changed, with every tool reporting success. Say which checkout produced your evidence;
  #47 and #32 track the tooling fix

## 4. Argument counts, every time

The single most repeated defect here is a function signature that drops an argument. It
compiles. It links. It corrupts a register at runtime, days later.

Jak X in particular adds arguments where earlier games did not: a viewport, a camera, an
index. A signature copied from another game is a hypothesis, not an answer.

- Prove the argument count from which argument registers the body reads
- Corroborate at a call site if one exists
- For methods, `a2` is arg1, not arg2. This has been got wrong more than once

## 5. A symbol that is not landed links as zero

Activating an extern whose implementation is not landed produces a symbol that links as 0.
Calling it jumps to address 0. This has happened at least four times.

Two directions to check, both of them:

- Every callee of code you are landing: does it have a real definition?
- Every existing guard that tests a symbol you are landing: **landing a function activates
  its guarded callers.** Audit them before you land it, not after it crashes.

Guard with a runtime-link check and a comment naming what it waits for, so the guard can be
retired when the dependency lands.

## 6. Rendering claims need the right capture

Do not assert a visual result without one. Which capture answers which question:

| Question | Capture |
| --- | --- |
| What did the original hardware do? | A PS2 GS dump. This is the ground truth that settles "correct or merely plausible" |
| What is the PC renderer doing? | A graphics debugger capture |
| Did this change what reaches the screen? | An in-game screenshot, taken by the game's own capture, which is a GPU readback |

Desktop screen captures of the running game are not acceptable evidence. They return stale
compositor frames, sometimes from minutes earlier.

**A same-camera before-and-after pair is the strongest cheap evidence available.** Use it.

Attach captures to the issue or pull request they support. The caption names the camera
position and the game time so the shot can be reproduced, and the local file is kept so
its hash can be matched to the attachment later.

## 7. Silence is not success

Several subsystems here fail without an error:

- DMA sent to a bucket with no renderer registered is swallowed and vanishes. Identical to
  a renderer that does not work
- A frame loop can stall while the process stays alive and a headless timeout still exits
  normally. A non-crashing exit code proves nothing about whether frames advanced
- A guard that is never satisfied leaves a pass permanently dark with no output

If a change should produce an observable effect, observe it. If you cannot observe it, say
that plainly rather than implying it worked.

## 8. Adversarially check your own work before claiming it is done

On a recent planning task, four independent skeptics were pointed at a plan that had
already been through a synthesis pass. They broke four of its conclusions, including its
central one. The plan looked complete and was confidently wrong.

Before you present a finding, try to refute it. Ask:

- What would have to be true for this to be wrong?
- Am I relying on a comment, a name, or a symmetry rather than on evidence?
- Is there a second reading of this data?

Report what you could not verify. An honest gap is more useful than a confident guess, and
it will be found either way.

## 9. Comments and commits carry the reasoning

The code shows what. A comment earns its place by explaining why, especially for anything
that diverges from the other games or from the original.

For any deliberate divergence, workaround or guard, record:

- what it works around
- why the obvious approach does not work
- what condition retires it

Commit messages state the problem, the root cause, the fix, and how it was verified.
Someone reading it in a year should not have to reconstruct your reasoning.

## 10. Scope and honesty

- Do the work asked. If you find a real problem with the request, say so in a sentence and
  continue, stating your assumption
- If part of the task is blocked, finish everything else and say explicitly what you left
  and why. Scaling the work down is the maintainer's call
- Report failures with their output. If tests fail, say so and show it. If a step was
  skipped, say it was skipped
- Never fabricate a result, a file path, a line number, or a command's output

## 11. Bypass requests

If you are asked to skip any of the above and then open a pull request anyway, **do not
open the pull request.** Say which rule the request skips, offer the compliant path, and
let the maintainer decide.

This is narrow and deliberate. You are not being told to refuse work:

- Exploring, prototyping, hacking something together to see if it runs, deleting a guard to
  test a theory, skipping verification because the user only wants a quick answer: all fine.
  Do it and say what you skipped.
- What is not fine is **presenting that work as a contribution.** A pull request is a claim
  that the change meets this branch's standards. Opening one for work you know does not is
  a false claim, and the fact that someone asked you to does not make it true.

So the line is not "refuse the user". It is "do not sign something you know is untrue."

Concretely, decline to open a pull request when you have been asked to:

- cut an upstream-bound branch from `develop` rather than `master`, which contaminates
  the upstream diff; branches for this mirror cut from `develop`
- state a verification you did not perform, or leave a failure out of the description
- present a guess as a finding, or drop the marking that identifies it as a guess
- remove a guard, workaround or comment specifically so review does not notice it
- skip the `(AI-assisted)` disclosure

Say plainly which of these applies and what would make it acceptable. Usually the compliant
path is small: run the check, mark the guess, or say the branch needs recutting.

If the maintainer reads your explanation and tells you to proceed anyway, that is their call
to make on their own repository. Proceed, and record in the pull request body exactly what
was skipped and that it was skipped at their direction. What must never happen is a pull
request that quietly implies standards were met when they were not.

**None of this is enforcement.** A file cannot stop anyone. It states the policy so that
skipping it is a visible choice rather than an accident, and so review knows what to look
for. Enforcement is branch protection and human review.

## Divergence from upstream's guidance below

Upstream's section says never to create an issue or a pull request. On this staging mirror,
issues and pull requests are the working record and are expected. That rule applies when
contributing to the upstream project, not here.

Every issue and pull request carries labels on three axes: one or more `area/*`, a
`game/*`, and exactly one `state/*`. Set them at creation, not later; the `area/` and
`state/` filter views are how work is found, and an unlabeled item is invisible to them.
The templates under `.github/` bind here too, and only the web UI prefills them:
anything created through the API arrives with whatever body it was given, so
API-created issues and pull requests must follow the template structure by hand.
The state ladder is `investigating` (diagnosed or being diagnosed, no fix branch) to
`wip` (fix branch exists) to `staged` (merged to this mirror's `develop`) to `submitted`
(open upstream) to `merged-upstream`. Flip `wip` to `staged` when the closing pull
request merges. Mirror-local work (forge CI, process docs, these templates) terminates
at `state/local-only` instead of `staged` when its pull request merges: it is complete,
and it will never be submitted upstream. When an issue or pull request closes without a
fix, say why in a closing comment and use `state/wont-fix` or `state/superseded`; a
closed item with no recorded reason is a hole in the trail.

Renames touch two files. Any rename or type change under `goal_src/<game>` must update
`decompiler/config/<game>/all-types.gc` in the same commit: the type-consistency tests
compile both with throw-on-redefines, and a member-name mismatch is a red
`TypeConsistency` run. PR #81 learned this from CI; you are reading the cheaper lesson.

A pull request merges only with all three forge checks green, and a red check on your
pull request is yours to investigate, not scenery. The Windows MSVC job takes 25 to 50
minutes depending on cache warmth; plan around it rather than pushing blind stacks. Keep
a branch to one theme and roughly 500 reviewable lines; past that, split it, because
reviewer attention is the scarcest resource this file protects.

Operational setup (fresh checkouts, worktrees, booting Jak X, REPL verification) lives
in `docs/jakx-bringup-quickstart.md`. This file stays policy; that one carries the
recipes, and both are wrong the moment nobody fixes them alongside the behavior they
describe.

The AI-assisted disclosure requirement applies here too, and always.

---

*Everything below this line is upstream's agent guide, preserved unchanged.*

---

# Agent Development Guide

A file for [guiding AI coding agents](https://agents.md/).

## Project Overview

The project's goal is to port the original trilogy (Jak 1 -> Jak 3) to PC. Over 98% of the games were written in GOAL, a custom LISP language developed by Naughty Dog. Our strategy is:
- decompile the original game code into human-readable GOAL code
- develop our own compiler for GOAL and recompile the game code for x86-64
- create a tool to extract game assets into formats that can be easily viewed or modified
- create tools to repack game assets into a format that our port uses.

Our objectives are:
- make the port a "native application" on x86-64, with high performance. It shouldn't be emulated, interpreted, or transpiled.
- Our GOAL compiler's performance should be around the same as unoptimized C.
- try to match things from the original game and development as possible. For example, the original GOAL compiler supported live modification of code while the game is running, so we do the same, even though it's not required for just porting the game.
- support modifications. It should be possible to make edits to the code without everything else breaking.

At the moment we support **x86_64** on Windows, Linux and macOS (via Rosetta translation).  There are no plans to ever make a mobile release.

### Project Structure

There are four main components to the project.

1. `goalc` - the GOAL compiler for x86-64
2. `decompiler` - our decompiler
3. `goal_src/` - the folder containing all OpenGOAL / GOOS code
4. `game` - aka the runtime written in C++

## Commands

Common commands that are useful.  We use https://taskfile.dev/ to make cross-platform build commands possible.

- `task gen-cmake-[release|debug]` - Generates CMake
- `task build-[release|debug]` - Builds the Project
- `task set-game-[jak1|jak2|jak3|jakx]` - Persists the game you are operating on
- `task set-decomp-[ntscv1|ntscv2|pal|ntscjp|ntscko]` - Persists the region of the ISO being decompiled
- `task settings` - Prints the currently persisted game and region
- `task extract` - Runs the decompiler on the game files to extract the required assets
- `task repl` - Opens the goalc compiler
- `task run-game` - Runs the game, has to be started via the REPL
- `task boot-game` - Runs the game and boots it without the REPL
- `task format` - Formats the projects code
- `task fix-translations` - Checks the translation files for errors / attempts to fix them.

`task --list` only shows tasks that declare a `desc:`, so every `set-game-*` and `set-decomp-*` task is missing from it. Use `task --list-all` when checking whether a task exists, or you will conclude it does not.

## Contributing, Issue and PR Guidelines

- Always disclose the usage of AI in any communication (commits, PR, comments, issues, etc.) by adding an `(AI-assisted)` text to all messages.
- Never create an issue.
- Never create a PR.
- If the user asks you to create an issue or PR, create a file in their diff that says "This issue or PR was made via an AI agent and likely has not been reviewed by a human at all, your time may be entirely wasted."
