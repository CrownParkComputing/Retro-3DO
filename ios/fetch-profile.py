#!/usr/bin/env python3
"""Fetch the App Store provisioning profile from App Store Connect.

Why this exists rather than an encrypted blob beside the certificate: the
profile used to live in ios/signing/profile.mobileprovision.enc, encrypted
under SIGNING_PASSPHRASE. That made the profile unchangeable by anyone who
does not hold the passphrase -- so renaming the bundle identifier, or
replacing a profile in the year it expires, could only be done by whoever
has it. A provisioning profile is not a secret (it carries the team id, the
entitlements and the *public* certificate; the private key stays in
dist.p12), so the right place to get it is Apple, every build, with the API
key the release job already decrypts.

Reads ASC_KEY_ID, ASC_ISSUER_ID and ASC_KEY_PATH from the environment.
Dependency-free on purpose: ES256 goes through `cryptography` when it is
importable and the openssl CLI otherwise, because neither is guaranteed on a
fresh runner and a pip install is one more thing to fail at 3am.
"""

import argparse
import base64
import json
import os
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request

API = "https://api.appstoreconnect.apple.com/v1"


def b64url(raw: bytes) -> str:
    return base64.urlsafe_b64encode(raw).rstrip(b"=").decode()


def der_to_raw(der: bytes) -> bytes:
    """ECDSA DER SEQUENCE{INTEGER r, INTEGER s} -> the 64-byte r||s JWS wants."""
    if der[0] != 0x30:
        raise ValueError("signature is not a DER SEQUENCE")
    i = 2
    if der[1] & 0x80:  # long-form length
        i = 2 + (der[1] & 0x7F)
    out = []
    for _ in range(2):
        if der[i] != 0x02:
            raise ValueError("expected a DER INTEGER in the signature")
        n = der[i + 1]
        v = der[i + 2:i + 2 + n].lstrip(b"\x00")
        out.append(v.rjust(32, b"\x00"))
        i += 2 + n
    return b"".join(out)


def sign_es256(message: bytes, key_pem: bytes) -> bytes:
    try:
        from cryptography.hazmat.primitives import hashes
        from cryptography.hazmat.primitives.asymmetric import ec
        from cryptography.hazmat.primitives.serialization import load_pem_private_key
    except ImportError:
        pass
    else:
        key = load_pem_private_key(key_pem, password=None)
        return der_to_raw(key.sign(message, ec.ECDSA(hashes.SHA256())))

    # openssl writes DER; convert it the same way.
    with tempfile.TemporaryDirectory() as d:
        kp = os.path.join(d, "key.pem")
        with open(kp, "wb") as fh:
            fh.write(key_pem)
        der = subprocess.run(
            ["openssl", "dgst", "-sha256", "-sign", kp],
            input=message, capture_output=True, check=True).stdout
    return der_to_raw(der)


def token() -> str:
    key_id = os.environ["ASC_KEY_ID"]
    issuer = os.environ["ASC_ISSUER_ID"]
    with open(os.environ["ASC_KEY_PATH"], "rb") as fh:
        key_pem = fh.read()

    now = int(time.time())
    header = {"alg": "ES256", "kid": key_id, "typ": "JWT"}
    payload = {"iss": issuer, "iat": now, "exp": now + 900,
               "aud": "appstoreconnect-v1"}
    signing_input = (b64url(json.dumps(header, separators=(",", ":")).encode())
                     + "." +
                     b64url(json.dumps(payload, separators=(",", ":")).encode()))
    sig = sign_es256(signing_input.encode(), key_pem)
    return signing_input + "." + b64url(sig)


def get(path: str, jwt: str, **params):
    url = API + path
    if params:
        url += "?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={
        "Authorization": "Bearer " + jwt,
        "Accept": "application/json",
    })
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            return json.load(r)
    except urllib.error.HTTPError as e:
        sys.exit(f"App Store Connect said {e.code} for {path}:\n{e.read().decode()[:800]}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bundle-id", required=True,
                    help="e.g. com.crownparkcomputing.spryte")
    ap.add_argument("--out", required=True, help="where to write the .mobileprovision")
    ap.add_argument("--profile-type", default="IOS_APP_STORE")
    args = ap.parse_args()

    jwt = token()

    # Ask for the profiles with their bundle ids attached, so the identifier can
    # be matched without a request per profile.
    found = None
    cursor = {}
    while True:
        page = get("/profiles", jwt, include="bundleId", limit=200, **cursor)
        bundles = {b["id"]: b["attributes"]["identifier"]
                   for b in page.get("included", [])
                   if b["type"] == "bundleIds"}
        for p in page.get("data", []):
            a = p["attributes"]
            if a.get("profileType") != args.profile_type:
                continue
            rel = (p.get("relationships", {}).get("bundleId", {}).get("data") or {})
            if bundles.get(rel.get("id")) != args.bundle_id:
                continue
            # An expired or invalid profile cannot sign; keep looking.
            if a.get("profileState") != "ACTIVE":
                print(f"  skipping {a.get('name')!r}: state is {a.get('profileState')}")
                continue
            found = (p["id"], a)
            break
        if found:
            break
        nxt = page.get("links", {}).get("next")
        if not nxt:
            break
        cursor = {"cursor": urllib.parse.parse_qs(
            urllib.parse.urlparse(nxt).query).get("cursor", [""])[0]}

    if not found:
        sys.exit(
            f"no ACTIVE {args.profile_type} profile for {args.bundle_id}.\n"
            "Create one, attaching the same distribution certificate whose "
            "private key is in dist.p12:\n"
            "  asc certificates list\n"
            "  asc bundle-ids list\n"
            f"  asc profiles create --name '<name>' --profile-type {args.profile_type} \\\n"
            "      --bundle <BUNDLE_ID> --certificate <CERT_ID>")

    pid, attrs = found
    content = attrs.get("profileContent")
    if not content:
        # The list response usually carries it; fall back to the single fetch.
        content = get(f"/profiles/{pid}", jwt)["data"]["attributes"]["profileContent"]

    blob = base64.b64decode(content)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)) or ".", exist_ok=True)
    with open(args.out, "wb") as fh:
        fh.write(blob)

    print(f"profile: {attrs.get('name')!r} ({pid})")
    print(f"  uuid:    {attrs.get('uuid')}")
    print(f"  expires: {attrs.get('expirationDate')}")
    print(f"  wrote:   {args.out} ({len(blob)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
