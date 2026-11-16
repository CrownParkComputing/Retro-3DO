# Getting Spryte onto the App Store

Everything in the tree is ready. What is left is the part no API key can do.

The record exists:

| | |
|---|---|
| App ID | `6835535735` |
| Version ID | `fabf6214-0235-4559-84c7-583201f0c67f` (1.0) |
| Bundle ID | `com.crownparkcomputing.spryte` (`JTN3AC3XQ9`) |
| SKU | `spryte-ios` |

`asc validate` reports **0 errors, 0 warnings, 0 blocking**. What is left is
only what no API can touch.

## 1. Still web-only

- **App Privacy.** The app collects nothing: no account, no analytics, no
  network. Answer "Data Not Collected". No API can set this, and `asc
  validate` does not check it.
- **App Information → Regulations and Permits → DSA trader status.** Already
  satisfied at account level, but confirm it on the new record.
- **Price.** Worth a thought rather than a default: the family is free, and a
  paid download is one of the few differences guideline 4.3(a) actually counts.
  Availability is already set to all 175 territories.

## 2. The listing is already filled

Description, keywords, promotional text, support URL, copyright, categories
(Games / Action / Adventure, secondary Utilities), content rights, the age
rating (all "none", which is accurate — the app ships only the hardware demo)
and the review notes and contact details are all set from this directory. To
change any of them, edit the file and re-run the matching `asc` command.

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
