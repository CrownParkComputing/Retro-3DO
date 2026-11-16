# Getting Spryte onto the App Store

Everything in the tree is ready. What is left is the part no API key can do.

## 1. Create the app record (web only, and it is the blocker)

No App Store Connect API key can create an app record — `asc apps create` does
not exist, and the Apple ID that can is the outlook.com one. Until this exists,
the `release` job's final step fails: the archive builds and signs correctly,
and `altool` then has nothing to upload to.

In App Store Connect → Apps → **+** → New App:

| Field | Value |
|---|---|
| Platform | iOS |
| Name | `Spryte` |
| Primary language | English (U.K.) |
| Bundle ID | `com.crownparkcomputing.spryte` — already created, ID `JTN3AC3XQ9` |
| SKU | anything unused, e.g. `spryte-ios` |
| User access | Full |

Then, still on the website, because these are web-only too:

- **App Privacy.** The app collects nothing: no account, no analytics, no
  network. Answer "Data Not Collected".
- **Age rating.**
- **App Information → Regulations and Permits → DSA trader status.** Already
  satisfied at account level, but confirm it on the new record.
- **Price.** Worth a thought rather than a default: the family is free, and a
  paid download is one of the few differences guideline 4.3(a) actually counts.

## 2. Fill the listing from this directory

```sh
APP=<new app id>
asc localizations update --version <version-id> --locale en-GB \
  --description "$(cat store/appstore/description-en-GB.txt)" \
  --keywords    "$(cat store/appstore/keywords-en-GB.txt)" \
  --promotional-text "$(cat store/appstore/promo-en-GB.txt)"
asc review details update --version <version-id> \
  --notes "$(cat store/appstore/review-notes.txt)"
```

## 3. Screenshots

There are none in the tree on purpose — see `screenshots/README.md`. The push
that built this commit captures them; download the `spryte-screenshots-*`
artifacts, **look at them** (a sideways shot is the right pixel count and
passes every automatic check), then upload one file per invocation with
`--replace` on the first only, and poll until every asset reads COMPLETE rather
than trusting the exit code.

## 4. Reply to the old record first

`store/appstore/retro3do-4.3a-reply.txt`, pasted into the Resolution Center
thread on app `6808591874`. There is no API route to post it; `asc web review
show` can only read the thread.

Do this **before** Spryte goes into review, not after. An unanswered 4.3(a)
thread with the extended-review notice on it, and a new app of the same concept
appearing from the same account, is what "repeated submissions" means. The
reply says the app is withdrawn and the replacement is a distinct product,
which is a different thing entirely from submitting it again.

## 5. Keep the siblings back

Retro-Spectrum (`6804262616`) and C64-Retro (`6800389617`) are both still in
PREPARE_FOR_SUBMISSION. Leave them there until Spryte is through. Submitting
another app of the same shape while a 4.3 thread is open makes the pattern
worse, and the reply in step 4 says in writing that we are holding them.

## 6. One build live

After a successful upload, expire the others: `asc builds expire --build-id
<id> --confirm`, one at a time, and never `--latest` — that picks the newest,
which is the one to keep.

## What is already done

- Bundle ID `com.crownparkcomputing.spryte` (`JTN3AC3XQ9`) created.
- Provisioning profile "Spryte AppStore" (`D3CGPJ7T68`) created against
  certificate `XTN39MM234`, the Apple Distribution cert whose private key is in
  `dist.p12`. Verified: it carries `beta-reports-active`, so TestFlight works.
- The profile is fetched from Apple at build time by `ios/fetch-profile.py`,
  so it no longer needs `SIGNING_PASSPHRASE` to change and cannot go stale.
- Name, icon, palette, listing copy and keywords are Spryte's own.
