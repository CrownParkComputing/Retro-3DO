#!/usr/bin/env bash
set -euo pipefail

: "${PLAY_ACCESS_TOKEN:?PLAY_ACCESS_TOKEN is required}"
: "${BUNDLE_PATH:?BUNDLE_PATH is required}"
: "${TRACK:?TRACK is required}"

package_name="com.fourdo.android"
api_root="https://androidpublisher.googleapis.com/androidpublisher/v3/applications/${package_name}"
upload_root="https://androidpublisher.googleapis.com/upload/androidpublisher/v3/applications/${package_name}"
auth_header="Authorization: Bearer ${PLAY_ACCESS_TOKEN}"
committed=false

edit_json=$(curl --fail --silent --show-error \
  --request POST \
  --header "$auth_header" \
  --header 'Content-Type: application/json' \
  --data '{}' \
  "${api_root}/edits")
edit_id=$(jq --exit-status --raw-output '.id' <<<"$edit_json")

cleanup_edit() {
  if [ "$committed" != true ]; then
    curl --silent --request DELETE --header "$auth_header" \
      "${api_root}/edits/${edit_id}" >/dev/null || true
  fi
}
trap cleanup_edit EXIT

bundle_json=$(curl --fail --silent --show-error \
  --request POST \
  --header "$auth_header" \
  --header 'Content-Type: application/octet-stream' \
  --data-binary "@${BUNDLE_PATH}" \
  "${upload_root}/edits/${edit_id}/bundles?uploadType=media")
version_code=$(jq --exit-status --raw-output '.versionCode' <<<"$bundle_json")
echo "Uploaded bundle as versionCode ${version_code}"

# Build the release-notes array from whatever play/whatsnew/whatsnew-<lang>
# files exist, so the track update carries the same text the listing
# workflow would show, without hand-maintaining a second copy here.
release_notes='[]'
for notes_file in play/whatsnew/whatsnew-*; do
  [ -f "$notes_file" ] || continue
  language=$(basename "$notes_file" | sed 's/^whatsnew-//')
  text=$(<"$notes_file")
  release_notes=$(jq --compact-output \
    --arg language "$language" \
    --arg text "$text" \
    '. + [{language: $language, text: $text}]' \
    <<<"$release_notes")
done

track_body=$(jq --null-input --compact-output \
  --argjson versionCode "$version_code" \
  --argjson releaseNotes "$release_notes" \
  '{releases: [{versionCodes: [($versionCode|tostring)], status: "completed", releaseNotes: $releaseNotes}]}')

curl --fail --silent --show-error \
  --request PUT \
  --header "$auth_header" \
  --header 'Content-Type: application/json' \
  --data "$track_body" \
  "${api_root}/edits/${edit_id}/tracks/${TRACK}" >/dev/null

curl --fail --silent --show-error \
  --request POST \
  --header "$auth_header" \
  --header 'Content-Type: application/json' \
  --data '{}' \
  "${api_root}/edits/${edit_id}:commit" >/dev/null
committed=true
echo "Published versionCode ${version_code} to ${TRACK} for ${package_name}"
