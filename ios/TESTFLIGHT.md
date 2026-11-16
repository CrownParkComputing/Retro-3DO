# Getting the shelf build into TestFlight

Everything that can be done away from a Mac is done. What is left needs
macOS, because no Swift in this app has ever been compiled.

## Why not CI

`.github/workflows/ios.yml` does all of this on `macos-26` already, but the
`CrownParkComputing` Actions budget has been exhausted since 2026-11-11 and
every job fails in about two seconds with no log. Lifting it is web-only, at
github.com/settings/billing. If it is lifted, the workflow triggers on pushes
to `main` only, so either merge `ios-shelf` or add the branch to its `on:`.

## On the Mac

Needs the `SIGNING_PASSPHRASE` that encrypts `ios/signing/*.enc`. It is a
GitHub Actions secret and is **not** in `~/secrets.env`.

```sh
git clone -b ios-shelf https://github.com/CrownParkComputing/Retro-3DO.git
cd Retro-3DO && git submodule update --init --recursive
# The repository is still named Retro-3DO; the iOS app it builds is Spryte.

# 1. Does the Swift compile at all? Nothing has ever built it.
TARGET=simulator ./ios/build-ios-macos.sh

# 2. Decrypt the signing material. The provisioning profile is NOT among it:
#    it is fetched from Apple in step 2b, so that renaming the bundle
#    identifier does not need the passphrase and a profile cannot go stale.
D=$(mktemp -d); export PASS='<the passphrase>'
for f in dist.p12 asc_key.p8 signing.env; do
  openssl enc -d -aes-256-cbc -pbkdf2 -iter 600000 \
    -in "ios/signing/$f.enc" -out "$D/$f" -pass env:PASS
done
. "$D/signing.env"

# 2b. Fetch the profile with the API key just decrypted.
ASC_KEY_ID="$ASC_KEY_ID" ASC_ISSUER_ID="$ASC_ISSUER_ID" \
ASC_KEY_PATH="$D/asc_key.p8" \
  python3 ios/fetch-profile.py \
    --bundle-id com.crownparkcomputing.spryte \
    --out "$D/profile.mobileprovision"

security create-keychain -p build build.keychain
security default-keychain -s build.keychain
security unlock-keychain -p build build.keychain
security import "$D/dist.p12" -k build.keychain -P "$CERT_PASSWORD" -T /usr/bin/codesign
# Without the partition list every signing attempt fails with
# errSecInternalComponent, which reads like a broken certificate and is not.
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k build build.keychain
mkdir -p ~/Library/MobileDevice/Provisioning\ Profiles
cp "$D/profile.mobileprovision" ~/Library/MobileDevice/Provisioning\ Profiles/

# 3. Archive. The build number must be one the record has never seen.
ARCHIVE=1 \
RETRO3DO_MARKETING_VERSION=1.1 \
RETRO3DO_BUILD_NUMBER="$(date -u +%Y%m%d%H%M)" \
  ./ios/build-ios-macos.sh

# 4. Upload.
xcrun altool --upload-app -f build-ios/Spryte.ipa -t ios \
  --apiKey "$ASC_KEY_ID" --apiIssuer "$ASC_ISSUER_ID"
```

## Afterwards, from here

Processing takes a few minutes, then:

```sh
asc builds list --app 6808591874          # wait for processingState VALID
```

The **Internal** group (`32c15a09-9af0-4edc-9389-7606dae89d28`) has
`hasAccessToAllBuilds: true`, so the build reaches internal testers as soon as
it is valid, with no beta review.

The **External** group (`96708600-99b7-48be-aea6-e75948d64ef4`, public link
`https://testflight.apple.com/join/RFvPFzvt`, limit 20) needs beta app review,
and that approval is **per version train** — so a 1.1 build needs a fresh one
even though 1.0 was reviewed before.

## State of the record, 2026-11-14

All six existing builds read `expired: true`, including `5b6eaedc`
(202609290702), the one attached to the rejected version 1.0. Testers have
nothing installable until this upload lands.

Marketing version 1.1 is suggested above rather than 1.0 deliberately: it
starts a clean pre-release train for what is a genuinely different app, and it
keeps this work away from the rejected 1.0 record. Building as 1.0 instead
would join the existing train — the only practical difference is which train
the External group's beta review attaches to.
