#!/usr/bin/env python3
"""Fail a change that lets a later, weaker declaration silently win.

Why this exists: decompiler/config/jakx/all-types.gc loads top to bottom, and
when the same symbol is declared more than once, whichever declaration is LAST
in the file is the one the compiler actually uses. Nothing marks this as an
error: a bulk "provisional: call-site evidence only" batch, added far below an
earlier, evidence-backed declaration, silently overrides it. The evidence-backed
declaration keeps reading as though it is in effect, while the compiler is
quietly using the bare `object` (or a weaker `function`) guess instead. Several
of these were found by hand across recent rungs (*script-form*,
*async-request-ct*, *powerup-static-hash*, *camera-old-cpu*, and others) only
because someone happened to grep for the exact symbol; nothing caught the class
itself.

This script reads the file for real (not just greps it): it strips `#| ... |#`
block comments and `;;` line comments the same way the GOAL reader would, then
parses every ACTIVE define-extern, deftype and declare-type form. A name
declared more than once is a finding unless every declaration is textually
identical (comments aside), which is noted separately as benign: those are
still duplicate lines worth cleaning up eventually, but nothing is silently
losing information.

Three modes:

  report    Inventory every duplicate name in the tree (or --ref, for
            inspecting history). Always exits 0. This is the audit.

  absolute  Exit 1 if the tree carries ANY conflicting duplicate. This is the
            gate phase B flips on once the backlog this audit found is zero.
            Not used in CI yet.

  diff      The ratchet actually wired into CI. Exit 1 only if the CURRENT
            tree has a conflicting duplicate on a NAME that was not already
            conflicting at --base. Pre-existing backlog is not this change's
            problem, or no branch could land until phase B finishes the whole
            file; only NEW shadowing added by the change under test fails.
            The base copy is fetched with `git show <base>:<path>`, the same
            base-ref-via-git-show pattern check_deferred_markers.py's diff
            mode and the CI ledger step both already use.

Reading is CRLF-safe throughout: every source (working tree or git show) is
read as raw bytes, decoded, and split with str.splitlines(), which treats
CRLF, LF and lone CR uniformly and never leaves a stray \\r stuck to a line's
content. That stray \\r has previously made two otherwise-identical
declarations compare as different.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

DEFAULT_PATH = "decompiler/config/jakx/all-types.gc"

# The only three declaration shapes we track, and only at column 0: every
# active (non-commented) occurrence of these three forms in this file starts
# at the beginning of its line, verified against the current tree before
# writing this parser. A commented-out one is indented under `;; ` instead
# and so never matches these prefixes.
FORM_KEYWORDS = ("define-extern", "deftype", "declare-type")
FORM_PREFIXES = tuple(f"({kw} " for kw in FORM_KEYWORDS)

# The placeholder tokens a provisional/bulk pass uses when it has nothing
# better than call-site evidence: a bare `object` for a global, a bare
# `function` for a function whose signature was not resolved. Either one
# losing to something more specific is progress; either one WINNING over
# something more specific is the defect this script exists to catch.
PLACEHOLDER_TYPESPECS = ("object", "function")


@dataclass
class Decl:
    kind: str  # "define-extern" | "deftype" | "declare-type"
    name: str
    line: int  # 1-based line the form starts on
    end_line: int  # 1-based line the form's parens close on (may equal line)
    detail: str  # the part that actually matters for comparison (see below)


@dataclass
class Finding:
    name: str
    winner: Decl
    losers: list[Decl]
    classes: list[str]  # one per loser, aligned with losers

    def worst_class(self) -> str:
        # later-weaker is the dangerous case: sort it first everywhere.
        order = {"later-weaker": 0, "incomparable": 1, "cross-kind": 2,
                 "conflicting": 3, "later-stronger": 4}

        def rank(c: str) -> int:
            for prefix, r in order.items():
                if c == prefix or c.startswith(prefix + ":") or c.startswith(prefix + "-"):
                    return r
            return 5

        return min(self.classes, key=rank)


# ---------------------------------------------------------------------------
# Reading


def read_bytes_from_disk(path: str) -> bytes:
    return Path(path).read_bytes()


def read_bytes_from_ref(ref: str, path: str) -> bytes:
    proc = subprocess.run(["git", "show", f"{ref}:{path}"], capture_output=True)
    if proc.returncode != 0:
        stderr = proc.stderr.decode("utf-8", errors="replace")
        raise SystemExit(
            f"FATAL: 'git show {ref}:{path}' failed (exit {proc.returncode}):\n{stderr}"
        )
    return proc.stdout


def to_lines(data: bytes) -> list[str]:
    # CRLF-safe: splitlines() treats \r\n, \n and lone \r all as one line
    # break and drops it, unlike a naive text.split("\n").
    return data.decode("utf-8", errors="replace").splitlines()


def warn_if_dirty() -> None:
    """Mirror the other gates: say so when uncommitted work exists.

    diff and absolute mode read the working tree directly (not HEAD) when no
    --ref override is given, so this is informational rather than a
    correctness warning the way it is in the other two scripts; it is here so
    a result is never silently describing something other than what the
    caller thinks it is describing.
    """
    proc = subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True,
                          encoding="utf-8", errors="replace")
    dirty = [ln for ln in proc.stdout.splitlines() if ln.strip()]
    if dirty:
        print(
            f"NOTE: {len(dirty)} uncommitted change(s) in the working tree.\n"
            "      report/absolute mode with no --ref reads the WORKING TREE, so\n"
            "      these ARE included; diff mode's CURRENT side does too. Only\n"
            "      --base (and an explicit --ref) is read from git history.\n",
            file=sys.stderr,
        )


# ---------------------------------------------------------------------------
# Parsing: strip #| ... |# block comments (always a bare delimiter alone on
# its own line in this file, verified against the current tree: 1265 open
# markers, 1265 close markers, balanced, never nested), then walk line by
# line collecting every active define-extern / deftype / declare-type form.


def strip_block_comments(lines: list[str]) -> list[str]:
    out = []
    in_block = False
    for ln in lines:
        s = ln.strip()
        if not in_block and s == "#|":
            in_block = True
            out.append("")
            continue
        if in_block:
            if s == "|#":
                in_block = False
            out.append("")
            continue
        out.append(ln)
    return out


def normalize(text: str) -> str:
    return " ".join(text.split())


def scan_form(lines: list[str], start: int) -> tuple[int, str]:
    """Collect a form starting at lines[start] until its parens balance.

    Strips trailing `;` line comments and honors string literals (so a `(`,
    `)` or `;` inside a docstring never perturbs paren-depth counting).
    Returns (index of the line the form closed on, joined form text).
    """
    depth = 0
    in_string = False
    escape = False
    collected: list[str] = []
    i = start
    n = len(lines)
    while i < n:
        line = lines[i]
        out_chars = []
        j = 0
        m = len(line)
        while j < m:
            c = line[j]
            if in_string:
                out_chars.append(c)
                if escape:
                    escape = False
                elif c == "\\":
                    escape = True
                elif c == '"':
                    in_string = False
                j += 1
                continue
            if c == '"':
                in_string = True
                out_chars.append(c)
                j += 1
                continue
            if c == ";":
                break  # rest of line is a comment
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            out_chars.append(c)
            j += 1
        collected.append("".join(out_chars))
        if depth <= 0:
            return i, "\n".join(collected)
        i += 1
    # Unterminated form (should not happen on a well-formed file). Return
    # what we collected so the caller can decide whether to warn.
    return n - 1, "\n".join(collected)


def top_level_tokens(s: str) -> list[str]:
    """Split an s-expression's contents into top-level tokens.

    A parenthesized sub-expression or a quoted string is kept whole as one
    token; everything else is split on whitespace.
    """
    tokens = []
    i = 0
    n = len(s)
    while i < n:
        while i < n and s[i].isspace():
            i += 1
        if i >= n:
            break
        if s[i] == '"':
            j = i + 1
            while j < n and s[j] != '"':
                if s[j] == "\\":
                    j += 1
                j += 1
            j = min(j + 1, n)
            tokens.append(s[i:j])
            i = j
        elif s[i] == "(":
            depth = 1
            j = i + 1
            while j < n and depth > 0:
                if s[j] == '"':
                    j += 1
                    while j < n and s[j] != '"':
                        if s[j] == "\\":
                            j += 1
                        j += 1
                    j += 1
                    continue
                if s[j] == "(":
                    depth += 1
                elif s[j] == ")":
                    depth -= 1
                j += 1
            tokens.append(s[i:j])
            i = j
        else:
            j = i
            while j < n and not s[j].isspace() and s[j] not in "()\"":
                j += 1
            tokens.append(s[i:j])
            i = j
    return tokens


def build_decl(kind: str, start_line: int, end_line: int, form_text: str) -> Decl | None:
    text = form_text.strip()
    if not (text.startswith("(") and text.endswith(")")):
        return None
    inner = text[1:-1].strip()
    tokens = top_level_tokens(inner)
    if len(tokens) < 2 or tokens[0] != kind:
        return None
    name = tokens[1]
    rest = tokens[2:]
    if kind == "define-extern":
        # NAME TYPESPEC, or NAME "docstring" TYPESPEC. The docstring never
        # affects what the compiler resolves the symbol to, so it is not
        # part of `detail`; only the typespec is.
        detail = normalize(rest[-1]) if rest else ""
    else:
        # deftype: NAME (PARENT) fields... options...  -> whole body matters.
        # declare-type: NAME PARENT                     -> the parent matters.
        detail = normalize(" ".join(rest))
    return Decl(kind=kind, name=name, line=start_line, end_line=end_line, detail=detail)


def parse_declarations(lines: list[str]) -> tuple[list[Decl], list[tuple[int, str]]]:
    lines = strip_block_comments(lines)
    decls: list[Decl] = []
    warnings: list[tuple[int, str]] = []
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        kw = None
        for cand, prefix in zip(FORM_KEYWORDS, FORM_PREFIXES):
            if line.startswith(prefix):
                kw = cand
                break
        if kw is None:
            i += 1
            continue
        start = i
        end, form_text = scan_form(lines, i)
        decl = build_decl(kw, start + 1, end + 1, form_text)
        if decl is None:
            warnings.append((start + 1, f"could not parse {kw} form: {form_text[:120]!r}"))
        else:
            decls.append(decl)
        i = end + 1
    return decls, warnings


# ---------------------------------------------------------------------------
# Classification


def is_compiler_protected(loser: Decl, winner: Decl) -> bool:
    """True when the type system itself already guards this pair, so it is
    not this checker's job (and flagging it would just be noise).

    Two GOAL idioms produce a cross-kind "duplicate" by this script's plain
    name-collision test, and neither is the shadowing defect:

    1. declare-type / deftype, either order. TypeSystem::forward_declare_type_as
       (common/type_system/TypeSystem.cpp) is called for every declare-type. If
       the type is not yet fully defined it registers a forward declaration;
       if the type IS already fully defined (the deftype came first) it
       type-checks the new parent against the existing one and either
       silently ignores the forward declaration (consistent) or throws
       (inconsistent). So a declare-type/deftype pair that survives in a
       tree that builds is provably safe, and one that is not would already
       fail the build and the JakXTypeConsistency gate long before this
       script runs. This is the common, intentional forward-declare-before-
       define pattern and appears by the hundred; treating it as a finding
       would drown the real defect in noise.

    2. define-extern NAME type, paired with a deftype or declare-type for the
       same NAME. This is the bootstrap idiom the top of the file itself uses
       for object/type/structure/basic/etc: `(define-extern X type)` states
       that the symbol X's value is itself a type, which is exactly what
       deftype's own runtime-type registration (DecompilerTypeSystem.cpp,
       add_symbol(name, "type", ...)) also asserts. Both sides agree, so this
       is not a shadow. Only a define-extern typing the symbol as something
       OTHER than the literal token `type` (typically the bulk-provisional
       `object`) is a real conflict with a type declaration.
    """
    kinds = {loser.kind, winner.kind}
    if kinds == {"declare-type", "deftype"}:
        return True
    if "define-extern" in kinds and ("deftype" in kinds or "declare-type" in kinds):
        de = loser if loser.kind == "define-extern" else winner
        if de.detail == "type":
            return True
    return False


def parse_function_args(typespec: str) -> list[str] | None:
    if not (typespec.startswith("(") and typespec.endswith(")")):
        return None
    inner = typespec[1:-1].strip()
    toks = top_level_tokens(inner)
    if not toks or toks[0] != "function":
        return None
    return toks[1:]


def classify_typespec(earlier: str, later: str) -> str:
    """Classify how `later` compares to `earlier` for a define-extern pair."""
    later_ph = later in PLACEHOLDER_TYPESPECS
    earlier_ph = earlier in PLACEHOLDER_TYPESPECS
    if later_ph and not earlier_ph:
        return "later-weaker"
    if earlier_ph and not later_ph:
        return "later-stronger"

    e_args = parse_function_args(earlier)
    l_args = parse_function_args(later)
    if e_args is not None and l_args is not None:
        if len(l_args) < len(e_args):
            return "later-weaker"
        if len(l_args) > len(e_args):
            return "later-stronger"
        weaker_positions = 0
        stronger_positions = 0
        comparable = True
        for a, b in zip(e_args, l_args):
            if a == b:
                continue
            if b == "object" and a != "object":
                weaker_positions += 1
            elif a == "object" and b != "object":
                stronger_positions += 1
            else:
                comparable = False
        if not comparable:
            return "incomparable"
        if weaker_positions and not stronger_positions:
            return "later-weaker"
        if stronger_positions and not weaker_positions:
            return "later-stronger"
        return "incomparable"

    return "incomparable"


def classify_pair(loser: Decl, winner: Decl) -> str:
    if loser.kind != winner.kind:
        return f"cross-kind:{loser.kind}->{winner.kind}"
    if loser.kind != "define-extern":
        return f"conflicting-{loser.kind}"
    return classify_typespec(loser.detail, winner.detail)


def analyze(
    decls: list[Decl],
) -> tuple[list[Finding], list[tuple[str, list[Decl]]], list[tuple[str, list[Decl]]]]:
    """Returns (findings, benign_identical, compiler_protected).

    findings           conflicting duplicates: worth phase B's attention.
    benign_identical    every declaration for the name says the same thing.
    compiler_protected  a declare-type/deftype or define-extern-as-type chain
                        the compiler itself already validates; see
                        is_compiler_protected.
    """
    by_name: dict[str, list[Decl]] = {}
    for d in decls:
        by_name.setdefault(d.name, []).append(d)

    findings: list[Finding] = []
    benign: list[tuple[str, list[Decl]]] = []
    protected: list[tuple[str, list[Decl]]] = []
    for name, ds in by_name.items():
        if len(ds) < 2:
            continue
        ds_sorted = sorted(ds, key=lambda d: d.line)
        details = {d.detail for d in ds_sorted}
        if len(details) == 1:
            benign.append((name, ds_sorted))
            continue
        winner = ds_sorted[-1]
        all_losers = [d for d in ds_sorted[:-1] if d.detail != winner.detail]
        losers = [d for d in all_losers if not is_compiler_protected(d, winner)]
        if not losers:
            if all_losers:
                protected.append((name, ds_sorted))
            continue
        classes = [classify_pair(loser, winner) for loser in losers]
        findings.append(Finding(name=name, winner=winner, losers=losers, classes=classes))
    return findings, benign, protected


# ---------------------------------------------------------------------------
# Reporting


def truncate(s: str, n: int = 90) -> str:
    return s if len(s) <= n else s[: n - 3] + "..."


def format_finding_line(f: Finding) -> str:
    losers = "; ".join(
        f"line {loser.line} [{loser.kind}] {truncate(loser.detail)} ({cls})"
        for loser, cls in zip(f.losers, f.classes)
    )
    return (
        f"{f.name}: {losers} "
        f"-> WINS at line {f.winner.line} [{f.winner.kind}] {truncate(f.winner.detail)}"
    )


def format_benign_line(name: str, ds: list[Decl]) -> str:
    lines = ", ".join(str(d.line) for d in ds)
    return f"{name}: {len(ds)}x identical [{ds[0].kind}] {truncate(ds[0].detail)} @ lines {lines}"


def run_report(lines: list[str], label: str) -> int:
    warn_if_dirty()
    decls, warnings = parse_declarations(lines)
    findings, benign, protected = analyze(decls)
    findings.sort(key=lambda f: f.name)
    benign.sort(key=lambda kv: kv[0])
    protected.sort(key=lambda kv: kv[0])

    by_kind: dict[str, int] = {}
    for d in decls:
        by_kind[d.kind] = by_kind.get(d.kind, 0) + 1

    class_counts: dict[str, int] = {}
    for f in findings:
        c = f.worst_class()
        class_counts[c] = class_counts.get(c, 0) + 1

    total_dupe_names = len(findings) + len(benign) + len(protected)
    print(f"all-types shadowing audit: {label}")
    print(f"  active declarations parsed: {sum(by_kind.values())}"
          f" (define-extern {by_kind.get('define-extern', 0)},"
          f" deftype {by_kind.get('deftype', 0)},"
          f" declare-type {by_kind.get('declare-type', 0)})")
    print(f"  parse warnings: {len(warnings)}")
    for line_no, msg in warnings[:20]:
        print(f"    line {line_no}: {msg}")
    print(f"  names declared more than once: {total_dupe_names}")
    print(f"    conflicting (declarations differ): {len(findings)}")
    for cls in sorted(class_counts):
        print(f"      {cls}: {class_counts[cls]}")
    print(f"    benign (textually identical): {len(benign)}")
    print(f"    compiler-protected (declare-type/deftype forward-declare idiom "
          f"or define-extern-as-type bootstrap idiom, not this checker's concern): "
          f"{len(protected)}")
    print()

    if findings:
        print("CONFLICTING DUPLICATES (later declaration silently wins over an earlier one):")
        for f in findings:
            print(f"  {format_finding_line(f)}")
        print()

    if benign:
        print("benign identical duplicates (same effective declaration, listed for cleanup):")
        for name, ds in benign:
            print(f"  {format_benign_line(name, ds)}")
        print()

    return 0


def run_absolute(lines: list[str], label: str) -> int:
    warn_if_dirty()
    decls, warnings = parse_declarations(lines)
    findings, _benign, _protected = analyze(decls)
    findings.sort(key=lambda f: f.name)

    if warnings:
        print(f"note: {len(warnings)} form(s) could not be parsed; see --mode report for detail",
              file=sys.stderr)

    if not findings:
        print(f"all-types shadowing check ({label}): OK, no conflicting duplicates")
        return 0

    print(f"all-types shadowing check ({label}) FAILED: "
          f"{len(findings)} conflicting duplicate name(s)\n")
    for f in findings:
        print(f"  {format_finding_line(f)}")
    return 1


def run_diff(base: str, path: str) -> int:
    warn_if_dirty()
    current_lines = to_lines(read_bytes_from_disk(path))
    base_lines = to_lines(read_bytes_from_ref(base, path))

    current_decls, current_warnings = parse_declarations(current_lines)
    base_decls, _base_warnings = parse_declarations(base_lines)

    current_findings, _, _ = analyze(current_decls)
    base_findings, _, _ = analyze(base_decls)

    current_by_name = {f.name: f for f in current_findings}
    base_names = {f.name for f in base_findings}

    new_names = sorted(set(current_by_name) - base_names)

    if current_warnings:
        print(f"note: {len(current_warnings)} form(s) in the current tree could not be parsed",
              file=sys.stderr)

    if not new_names:
        print("all-types shadowing ratchet: OK "
              f"(no new conflicting duplicates relative to {base})")
        if current_findings:
            print(f"  (pre-existing backlog carried forward, not this change's problem: "
                  f"{len(current_findings)} name(s))")
        return 0

    print(f"all-types shadowing ratchet FAILED: {len(new_names)} NEW conflicting "
          f"duplicate(s) relative to {base}\n")
    for name in new_names:
        print(f"  {format_finding_line(current_by_name[name])}")
    print(
        "\nEach name above is declared more than once in decompiler/config/jakx/all-types.gc "
        "with differing effective types, and this pair is new on this branch (it was not "
        "already conflicting at the base ref). The file loads top to bottom, so whichever "
        "declaration is LAST wins silently; if that is the weaker one, the earlier "
        "evidence-backed declaration is being ignored. Either remove the shadowing "
        "declaration, or if both should stay, make them agree."
    )
    return 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mode", choices=("report", "absolute", "diff"), default="report")
    ap.add_argument("--base", default="origin/develop",
                    help="base ref for diff mode (default: origin/develop)")
    ap.add_argument("--ref", default=None,
                    help="inspect this git ref via 'git show' instead of the working tree "
                         "(report and absolute modes only)")
    ap.add_argument("--path", default=DEFAULT_PATH,
                    help="path to the all-types.gc to check (default: %(default)s)")
    args = ap.parse_args()

    if args.mode == "diff":
        return run_diff(args.base, args.path)

    if args.ref:
        lines = to_lines(read_bytes_from_ref(args.ref, args.path))
        label = args.ref
    else:
        lines = to_lines(read_bytes_from_disk(args.path))
        label = "working tree"

    if args.mode == "report":
        return run_report(lines, label)
    return run_absolute(lines, label)


if __name__ == "__main__":
    sys.exit(main())
