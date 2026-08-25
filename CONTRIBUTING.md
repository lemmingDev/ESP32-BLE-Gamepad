# Contributing

## Code style / linting

Linting here happens in two layers, from smallest/lowest-risk to largest:

1. **Line endings & whitespace** — [.editorconfig](.editorconfig) and [.gitattributes](.gitattributes),
   enforced automatically by your editor/git, no extra tool to install.
2. **C/C++ formatting** — [clang-format](https://clang.llvm.org/docs/ClangFormat.html), driven by
   [.clang-format](.clang-format) at the repo root, run manually or via your editor.

Both are checked in CI (`.github/workflows/lint.yml`) on every push and pull request, but neither **blocks**
merging — the existing codebase predates these rules and is still catching up. Please still try to follow
them in your own changes before opening a PR.

### Line endings & whitespace

[.editorconfig](.editorconfig) and [.gitattributes](.gitattributes) fix the most basic inconsistencies in
the repo: some files use Windows CRLF line endings instead of Unix LF, and a few have stray tab characters
instead of spaces.

- **.editorconfig** is picked up automatically by most editors (VS Code, Visual Studio, CLion, Sublime,
  vim/neovim via a small plugin, Notepad++ via a plugin) — it sets LF line endings, 4-space indentation, and
  trims trailing whitespace for you as you type. No install needed on macOS/Linux; on Windows, VS Code and
  Visual Studio support it out of the box, other editors may need a plugin (search "EditorConfig" in your
  editor's extension marketplace).
- **.gitattributes** tells git itself to normalize line endings to LF in the repository regardless of your
  platform's `core.autocrlf` setting, so this doesn't require any per-contributor configuration.

You can check for existing CRLF/tab issues yourself with (Git Bash/WSL/Linux/macOS shell — this uses only
`$'...'` literal matching so it works with both GNU grep and macOS's built-in BSD grep, no `-P`/`-U` needed):
```sh
# CRLF line endings
grep -rl $'\r' --include='*.cpp' --include='*.h' --include='*.hpp' --include='*.ino' . | grep -v '^\./test/'

# Tab characters
grep -rl $'\t' --include='*.cpp' --include='*.h' --include='*.hpp' --include='*.ino' . | grep -v '^\./test/'
```
This is the same check the CI "Lint / whitespace" job runs.

**Previewing the whitespace fix**, without touching any files:

CRLF diffs are misleading with a plain `diff` — the invisible `\r` makes *every* line look changed, even
though only the line ending differs. Use `git diff --ignore-space-at-eol` instead, which shows nothing if
CRLF removal is the only change (i.e. confirms the fix is purely cosmetic, no real content touched):
```sh
git diff --no-index --ignore-space-at-eol -- BleNUS.cpp <(sed 's/\r$//' BleNUS.cpp)
```

Tabs don't have that problem, so a plain diff already shows a minimal, readable result:
```sh
diff -u BleGamepadConfiguration.cpp <(sed 's/\t/ /g' BleGamepadConfiguration.cpp)
```

To see the raw invisible characters directly instead of a diff, `sed -n 'l'` prints `\r` and `\t` literally
and works the same on macOS (BSD sed) and Linux (GNU sed) — unlike `cat -A`, which macOS's built-in `cat`
doesn't support:
```sh
sed -n 'l' BleNUS.cpp | head                              # \r$ at the end of each line = CRLF
sed -n '137p' BleGamepadConfiguration.cpp | sed -n 'l'    # \t shown literally
```

(`<(...)` process substitution needs Git Bash/WSL/Linux/macOS shell, not plain PowerShell/cmd.)

### Installing clang-format

**macOS** (Homebrew):
```sh
brew install clang-format
```

**Linux** (Debian/Ubuntu):
```sh
sudo apt-get update
sudo apt-get install clang-format
```
Other distros: `sudo dnf install clang-tools-extra` (Fedora), `sudo pacman -S clang` (Arch), or use the
`pip install clang-format` package below.

**Windows**, any of:
- [LLVM installer](https://github.com/llvm/llvm-project/releases) — installs `clang-format.exe` and adds it
  to `PATH` (check the box during setup, or add `C:\Program Files\LLVM\bin` manually).
- `winget install LLVM.LLVM`
- Visual Studio: enable the "C++ Clang tools for Windows" component in the Visual Studio Installer.

**Any OS** (via pip, useful if you already have Python installed):
```sh
pip install clang-format
```

Verify it's on your `PATH`:
```sh
clang-format --version
```

### Using it

From the repo root:

```sh
# Check formatting without changing files (mirrors what CI runs)
find . \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.ino' \) -not -path './test/*' \
  -print0 | xargs -0 clang-format --dry-run --Werror -style=file

# Auto-fix formatting in place for files you've touched
clang-format -i -style=file BleGamepad.cpp BleGamepad.h
```

On Windows PowerShell, the fix command works the same way (`clang-format -i -style=file BleGamepad.cpp`);
the `find`/`xargs` check command needs a POSIX-style shell (Git Bash, WSL, or MSYS2).

**Previewing what would change**, without touching the file, as a normal unified diff:

```sh
diff -u BleNUS.cpp <(clang-format -style=file BleNUS.cpp)
```

(On Windows, run this from Git Bash or WSL — `<(...)` process substitution isn't available in
PowerShell/cmd.) This is the easiest way to see *why* two files disagree — e.g. comparing
`BleConnectionStatus.cpp` against `BleNUS.cpp` shows the former only has reference-alignment issues
(`NimBLEConnInfo& connInfo` → `NimBLEConnInfo &connInfo`), while the latter also mixes in K&R-style braces
(`if (...) {`) instead of Allman (`if (...)` then `{` on its own line), pointer alignment attached to the
type (`NimBLEServer*` vs `NimBLEServer *`), and trailing whitespace on blank lines — all of which
`clang-format -i` fixes in one pass.

### Testing your changes before pushing

To check only the files you've actually modified (much faster than scanning the whole repo), diff against
`master`:

```sh
git diff --name-only --diff-filter=ACMR master... -- '*.cpp' '*.h' '*.hpp' '*.ino' \
  | xargs -r clang-format --dry-run --Werror -style=file
```

This is the same check CI runs, just scoped to your branch. A clean run prints nothing and exits `0`.

**Reading the output** — each violation is reported as `file:line:col: error: code should be
clang-formatted`, followed by the offending line and a `^` pointing at the exact column, e.g.:

```
BleGamepad.h:66:11: error: code should be clang-formatted [-Wclang-format-violations]
    BleNUS* nus;
          ^
```

You don't need to hand-fix these — run `clang-format -i -style=file <file>` on the flagged file(s) and
re-run the check to confirm it's clean.

**Seeing CI's feedback on a PR** — once you push, open the pull request and look for the "Lint" workflow's
checks (under the *Checks* tab or the status list at the bottom of the PR): "Lint / whitespace" for line
endings and tabs, "Lint / clang-format" for formatting. Because both are non-blocking, they may show a
warning icon rather than a red X even when they find issues — click *Details* on either to open its Actions
log, which lists the affected files (and, for clang-format, the same `file:line:col` output shown above).

### Editor integration (optional)

Most editors will pick up `.clang-format` automatically once the extension/plugin is installed:
- **VS Code**: [Clang-Format extension](https://marketplace.visualstudio.com/items?itemName=xaver.clang-format), or enable `C_Cpp.clang_format_style: file` if using the Microsoft C/C++ extension.
- **CLion / other JetBrains IDEs**: built-in support — enable it under *Settings → Editor → Code Style → C/C++ → Enable ClangFormat*.
- **Vim/Neovim**: `clang-format.py`/`clang-format.vim` shipped with LLVM, or format-on-save via your LSP client.
