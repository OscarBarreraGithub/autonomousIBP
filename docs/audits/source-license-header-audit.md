# Source License Header Audit

Date: 2026-06-12

Requested starting HEAD: `df4c80c8467d0e09a83a868bc012e5afac25017f`

Rebased audit base: `abc7c3c1552643a0c6e2d1d6f0b3b0ca281921b9`

## Licensing Posture

No root `LICENSE`, `COPYING`, or `NOTICE` file is present at the requested HEAD
or at the rebased audit base. This audit records that as a known gap only; it
does not infer a project license and does not add a replacement license
statement.

## Header State

The audited surface covers:

- C++ source and header files under `lib/`, `include/`, `src/`, and `tools/`
- Python tools under `tools/`
- tracked `CMakeLists.txt` and `*.cmake` files

At the requested HEAD, the audited surface had 170 files and none carried a
recognized license or copyright header. After rebasing over the compile-warning
baseline lane, the current pinned surface has 171 missing-header files; the new
license-header verifier added by this lane carries an SPDX marker and is not in
the missing-header baseline. The CTest gate therefore pins the current
missing-header set in
`tools/reference-harness/specs/license/source-license-header-baseline.txt`
instead of editing existing source files.

## Gate Policy

The CTest gate accepts the existing missing-header files only while they remain
listed in the baseline. A newly tracked scoped source file must include an
expected header marker in its first 40 lines:

- `SPDX-License-Identifier:`
- `Copyright`
- `Licensed under`
- `Distributed under`

If an existing allowlisted file later gains a header, the baseline must be
updated to remove that path so the pin remains a current-state audit rather than
an unbounded exception list.
