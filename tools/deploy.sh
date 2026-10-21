#!/bin/sh
# Deploys site/ to S3 and refreshes CloudFront: deploy.sh <bucket> <distribution id> <version>
# Used by the GitHub workflow and for manual deploys. The page is served
# no-cache and points at its script and stylesheet with ?v=<version>, so a
# fresh page never runs an older cached app.js (or the reverse).
set -eu
bucket=$1 dist=$2 version=$3
out=build/site-deploy
rm -rf "$out" && mkdir -p "$out" && cp -R site/. "$out"/
sed -i.bak -E "s/(src|href)=\"(app\.js|bh\.js|style\.css)\"/\1=\"\2?v=$version\"/g" "$out/index.html" && rm "$out/index.html.bak"
aws s3 sync "$out"/ "s3://$bucket/" --delete --exclude "index.html" --exclude "bh.wasm" --cache-control "public, max-age=300"
aws s3 cp "$out/bh.wasm" "s3://$bucket/bh.wasm" --content-type application/wasm --cache-control "public, max-age=300"
aws s3 cp "$out/index.html" "s3://$bucket/index.html" --content-type "text/html; charset=utf-8" --cache-control "no-cache"
aws cloudfront create-invalidation --distribution-id "$dist" --paths "/*" --query Invalidation.Id --output text
