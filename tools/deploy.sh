#!/bin/sh
# Deploys site/ to S3 and refreshes CloudFront: deploy.sh <bucket> <distribution id> <version>
# Used by the GitHub workflow and for manual deploys. Pages are served
# no-cache and point at their scripts and stylesheet with ?v=<version>, so a
# fresh page never runs an older cached script (or the reverse). Pages other
# than the home page are uploaded without their .html, so they have clean
# URLs: algorithm.html is served at /algorithm.
set -eu
bucket=$1 dist=$2 version=$3
out=build/site-deploy
rm -rf "$out" && mkdir -p "$out" && cp -R site/. "$out"/
for page in "$out"/*.html; do
  sed -i.bak -E "s/(src|href)=\"([a-z]+\.js|style\.css)\"/\1=\"\2?v=$version\"/g" "$page" && rm "$page.bak"
done
aws s3 sync "$out"/ "s3://$bucket/" --delete --exclude "*.html" --exclude "algorithm" --exclude "bitmap" --exclude "bh.wasm" --cache-control "public, max-age=300"
aws s3 cp "$out/bh.wasm" "s3://$bucket/bh.wasm" --content-type application/wasm --cache-control "public, max-age=300"
for page in "$out"/*.html; do
  name=$(basename "$page" .html)
  [ "$name" = index ] && key=index.html || key=$name
  aws s3 cp "$page" "s3://$bucket/$key" --content-type "text/html; charset=utf-8" --cache-control "no-cache"
done
aws cloudfront create-invalidation --distribution-id "$dist" --paths "/*" --query Invalidation.Id --output text
