#!/usr/bin/env sh
set -eu

BUILD_DIR="${1:-build}"
OUTPUT_PATH="${2:-$BUILD_DIR/project-stats.md}"
OUTPUT_HTML_PATH="${3:-$BUILD_DIR/project-stats.html}"
REPO_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUTPUT_FULL="$OUTPUT_PATH"
OUTPUT_HTML_FULL="$OUTPUT_HTML_PATH"

case "$OUTPUT_FULL" in
    /*) ;;
    *) OUTPUT_FULL="$REPO_ROOT/$OUTPUT_FULL" ;;
esac

case "$OUTPUT_HTML_FULL" in
    /*) ;;
    *) OUTPUT_HTML_FULL="$REPO_ROOT/$OUTPUT_HTML_FULL" ;;
esac

mkdir -p "$(dirname -- "$OUTPUT_FULL")"
mkdir -p "$(dirname -- "$OUTPUT_HTML_FULL")"

count_scope() {
    root="$1"
    shift

    if [ ! -d "$root" ]; then
        printf "0 0\n"
        return
    fi

    find "$root" -type f "$@" \
        \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' \
        -o -name '*.h' -o -name '*.hh' -o -name '*.hpp' -o -name '*.hxx' \
        -o -name '*.inl' -o -name '*.ipp' \
        -o -name '*.vert' -o -name '*.frag' -o -name '*.glsl' \
        -o -name '*.cmake' -o -name '*.txt' -o -name '*.md' \
        -o -name '*.json' -o -name '*.yml' -o -name '*.yaml' \
        -o -name '*.ps1' -o -name '*.sh' -o -name '*.mk' \
        -o -name 'Makefile' -o -name 'CMakeLists.txt' \) \
        -print | awk '
            {
                files += 1
                while ((getline line < $0) > 0) {
                    lines += 1
                }
                close($0)
            }
            END {
                printf "%d %d\n", files, lines
            }
        '
}

git_section() {
    name="$1"
    root="$2"

    printf "### %s\n\n" "$name"
    if [ ! -d "$root/.git" ]; then
        printf -- "- Git stats unavailable.\n\n"
        return
    fi

    (
        cd "$root"
        branch="$(git branch --show-current 2>/dev/null || true)"
        if [ -z "$branch" ]; then
            branch="(detached)"
        fi

        printf -- "- Branch: \`%s\`\n" "$branch"
        printf -- "- Commits: %s\n" "$(git rev-list --count HEAD 2>/dev/null || printf 0)"
        printf -- "- Latest commit: %s\n" "$(git log -1 --format='%h %ad %s' --date=short 2>/dev/null || true)"
        printf -- "- First commit: %s\n" "$(git log --reverse --format='%h %ad %s' --date=short 2>/dev/null | sed -n '1p')"
        printf -- "- Dirty files: %s\n" "$(git status --short 2>/dev/null | wc -l | tr -d ' ')"
        printf "\n"
    )
}

nikreon_result="$(count_scope "$REPO_ROOT" \
    ! -path "$REPO_ROOT/.git/*" \
    ! -path "$REPO_ROOT/.agents/*" \
    ! -path "$REPO_ROOT/.codex/*" \
    ! -path "$REPO_ROOT/.vs/*" \
    ! -path "$REPO_ROOT/build/*" \
    ! -path "$REPO_ROOT/out/*" \
    ! -path "$REPO_ROOT/external/*" \
    ! -path "$REPO_ROOT/vcpkg_installed/*")"
nikreon_files="$(printf "%s" "$nikreon_result" | awk '{print $1}')"
nikreon_lines="$(printf "%s" "$nikreon_result" | awk '{print $2}')"

ui_root="$REPO_ROOT/external/NikreonUI"
ui_result="$(count_scope "$ui_root" \
    ! -path "$ui_root/.git/*" \
    ! -path "$ui_root/build/*" \
    ! -path "$ui_root/out/*" \
    ! -path "$ui_root/.vs/*")"
ui_files="$(printf "%s" "$ui_result" | awk '{print $1}')"
ui_lines="$(printf "%s" "$ui_result" | awk '{print $2}')"

combined_files=$((nikreon_files + ui_files))
combined_lines=$((nikreon_lines + ui_lines))

{
    printf "# Nikreon Project Stats\n\n"
    printf "Generated: %s\n\n" "$(date '+%Y-%m-%d %H:%M:%S %z')"
    printf "## Summary\n\n"
    printf "| Scope | Files | Lines |\n"
    printf "| --- | ---: | ---: |\n"
    printf "| Nikreon | %s | %s |\n" "$nikreon_files" "$nikreon_lines"
    printf "| NikreonUI | %s | %s |\n" "$ui_files" "$ui_lines"
    printf "| Combined | %s | %s |\n\n" "$combined_files" "$combined_lines"
    printf "## Git\n\n"
    git_section "Nikreon" "$REPO_ROOT"
    git_section "NikreonUI" "$ui_root"
    printf "## Notes\n\n"
    printf -- "- Counts include text/source files and skip generated build output, vcpkg, and git metadata.\n"
    printf -- "- The PowerShell version includes richer category and largest-file tables on Windows.\n"
} > "$OUTPUT_FULL"

{
    printf "<!doctype html>\n"
    printf "<html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
    printf "<title>Nikreon Project Stats</title>\n"
    printf "<style>body{margin:0;background:#11161f;color:#e8edf7;font:14px/1.45 system-ui,sans-serif}main{width:min(1100px,calc(100vw - 40px));margin:0 auto;padding:32px 0}h1{font-size:34px;margin:0 0 4px}h2{font-size:20px;margin:28px 0 10px}.muted{color:#9eabc1}.cards{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:12px;margin:22px 0}.card,section{background:#192231;border:1px solid #344158;border-radius:8px;padding:16px}.value{font-size:28px;font-weight:700;margin-top:6px}table{width:100%%;border-collapse:collapse}th,td{border-bottom:1px solid #344158;padding:8px 10px;text-align:left}th{color:#9eabc1;background:#202b3d}td:nth-child(2),td:nth-child(3),th:nth-child(2),th:nth-child(3){text-align:right}@media(max-width:800px){.cards{grid-template-columns:1fr}}</style>\n"
    printf "</head><body><main>\n"
    printf "<h1>Nikreon Project Stats</h1><p class=\"muted\">Generated: %s</p>\n" "$(date '+%Y-%m-%d %H:%M:%S %z')"
    printf "<div class=\"cards\"><article class=\"card\"><div class=\"muted\">Combined Lines</div><div class=\"value\">%s</div></article><article class=\"card\"><div class=\"muted\">Combined Files</div><div class=\"value\">%s</div></article><article class=\"card\"><div class=\"muted\">Combined Commits</div><div class=\"value\">See Git</div></article></div>\n" "$combined_lines" "$combined_files"
    printf "<section><h2>Summary</h2><table><thead><tr><th>Scope</th><th>Files</th><th>Lines</th></tr></thead><tbody>"
    printf "<tr><td>Nikreon</td><td>%s</td><td>%s</td></tr>" "$nikreon_files" "$nikreon_lines"
    printf "<tr><td>NikreonUI</td><td>%s</td><td>%s</td></tr>" "$ui_files" "$ui_lines"
    printf "<tr><td>Combined</td><td>%s</td><td>%s</td></tr>" "$combined_files" "$combined_lines"
    printf "</tbody></table></section>\n"
    printf "<section><h2>Notes</h2><p class=\"muted\">The PowerShell version includes richer category, largest-file, and declaration tables on Windows.</p></section>\n"
    printf "</main></body></html>\n"
} > "$OUTPUT_HTML_FULL"

printf "Project stats written to %s\n" "$OUTPUT_FULL"
printf "Project stats HTML written to %s\n" "$OUTPUT_HTML_FULL"
