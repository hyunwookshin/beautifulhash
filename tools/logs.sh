#!/bin/bash
# Summarizes the site's CloudFront access logs: page loads, unique visitors,
# referrers, and the edge locations that served them.
#
#   tools/logs.sh [days]     (default: 7)
#
# Logs are CloudFront standard logs in s3://beautifulhash-logs/cloudfront/,
# synced to build/logs. A page load is a GET of the page itself with status
# 200 or 304 from an address that also loaded the page's scripts, so
# scanners, link previews, and command-line tools are counted apart. Visitors
# are distinct IP addresses, so people behind one network count once and a
# phone that changes networks counts twice.
set -euo pipefail

days="${1:-7}"
bucket="s3://beautifulhash-logs/cloudfront/"
dir="build/logs"

mkdir -p "$dir"
aws s3 sync --quiet "$bucket" "$dir"

# Log files are named <distribution>.<YYYY-MM-DD-HH>.<id>.gz, in UTC.
since=$(date -u -v-"$((days - 1))"d +%Y-%m-%d 2>/dev/null || date -u -d "$((days - 1)) days ago" +%Y-%m-%d)
files=()
for f in "$dir"/*.gz; do
    [ -e "$f" ] || continue
    stamp=$(basename "$f" | cut -d. -f2 | cut -c1-10)
    [[ "$stamp" < "$since" ]] || files+=("$f")
done
if [ ${#files[@]} -eq 0 ]; then
    echo "No logs since $since (UTC) yet."
    exit 0
fi

gzip -dc "${files[@]}" | awk -F'\t' -v since="$since" '
    /^#/ { next }
    {
        # Fields: 1 date, 3 edge location, 5 client IP, 6 method, 8 path,
        # 9 status, 10 referrer, 11 user agent.
        requests++
        ok = $9 == 200 || $9 == 304
        # A real browser also loads the page'"'"'s scripts; scanners and link
        # previews fetch the page alone.
        if ($6 == "GET" && ok && $8 ~ /^\/(app\.js|bh\.js|bh\.wasm|style\.css)$/) browser[$5] = 1
        if (!($6 == "GET" && ($8 == "/" || $8 == "/index.html") && ok)) next
        pages++
        ip[pages] = $5; date[pages] = $1; ref[pages] = $10; edge[pages] = substr($3, 1, 3)
        bot[pages] = tolower($11) ~ /bot|crawl|spider|slurp|curl|wget|python|go-http|headless|preview|facebookexternalhit/
    }
    END {
        for (i = 1; i <= pages; i++) {
            if (bot[i] || !(ip[i] in browser)) { other++; continue }
            loads++
            day[date[i]]++
            if (!((date[i], ip[i]) in seen_day)) { seen_day[date[i], ip[i]] = 1; uniq_day[date[i]]++ }
            if (!(ip[i] in seen)) { seen[ip[i]] = 1; uniq++ }
            r = ref[i]
            sub(/^https?:\/\//, "", r); sub(/[\/?].*$/, "", r)
            if (r == "-" || r ~ /(^|\.)beautifulhash\.com$/) r = "(direct, app, or the site itself)"
            refs[r]++
            edges[edge[i]]++
        }
        printf "Since %s (UTC): %d requests, %d page loads by %d visitors (%d bot, tool, or scanner loads apart)\n\n", since, requests, loads, uniq, other
        if (!loads) exit
        print "Day (UTC)     Loads  Visitors"
        n = 0; for (d in day) keys[++n] = d
        for (i = 1; i <= n; i++) for (j = i + 1; j <= n; j++) if (keys[j] < keys[i]) { t = keys[i]; keys[i] = keys[j]; keys[j] = t }
        for (i = 1; i <= n; i++) printf "%s  %6d  %8d\n", keys[i], day[keys[i]], uniq_day[keys[i]]
        print "\nReferrers"
        for (r in refs) printf "%6d  %s\n", refs[r], r | "sort -rn | head -15"
        close("sort -rn | head -15")
        print "\nEdge locations (airport codes, a rough guide to region)"
        for (e in edges) printf "%6d  %s\n", edges[e], e | "sort -rn | head -15"
    }
'
