# Guix Build & Attestation (server build & laptop signing)

Build Bitcoin Core release binaries with Guix on the server, attest the hashes,
sign them with my builder key on the laptop, and open a PR to
[bitcoin-core/guix.sigs](https://github.com/bitcoin-core/guix.sigs).

Set these first. Every command below uses them:

```bash
export VERSION=32.0rc4     # no leading "v" (used for guix.sigs dir names)
export TAG=v$VERSION       # git tag in bitcoin / bitcoin-detached-sigs
export SIGNER=yuvicc
```

### What gets produced

| File | Comes from | Contains |
|------|-----------|----------|
| `noncodesigned.SHA256SUMS` | `guix-build` | Linux tarballs, unsigned Windows/macOS outputs, `*-codesigning.tar.gz` |
| `all.SHA256SUMS` | `guix-build` **+** `guix-codesign` | All of the above **plus** the signed `win64-setup.exe`, `win64.zip`, the macOS `.zip`/`.tar.gz` and `codesignatures-*.tar.gz` |
| `*.SHA256SUMS.asc` | `gpg` (laptop) | Detached signatures of the two files above |

`all.SHA256SUMS` only appears once codesigning has been done. If it's missing,
step 6 was skipped or failed.

## 1. Prerequisites (server)

```bash
sudo apt update && sudo apt install uidmap gnupg curl xz-utils git tmux
```

On Ubuntu 24.04 and later, AppArmor blocks the unprivileged user namespaces
that the Guix daemon needs:

```bash
sudo sysctl -w kernel.apparmor_restrict_unprivileged_userns=0
# make it persistent across reboots
echo 'kernel.apparmor_restrict_unprivileged_userns=0' | sudo tee /etc/sysctl.d/99-guix.conf
```

## 2. Install Guix

Use the distro package or the official install script:

```bash
sudo apt install guix
# or:
# cd /tmp && wget https://guix.gnu.org/install.sh && chmod +x install.sh && sudo ./install.sh
```

```bash
sudo systemctl status guix-daemon
```

```bash
. /etc/profile.d/guix.sh
guix --version
```

```bash
guix pull
hash guix
guix --version
```

```bash
guix archive --authorize < ~/.config/guix/current/share/guix/ci.guix.gnu.org.pub
guix archive --authorize < ~/.config/guix/current/share/guix/bordeaux.guix.gnu.org.pub
```

```bash
guix install glibc-locales
```

## 3. Source & repos

First time only:

```bash
git clone https://github.com/bitcoin/bitcoin.git ~/bitcoin
git clone https://github.com/bitcoin-core/guix.sigs.git ~/guix.sigs
git clone https://github.com/bitcoin-core/bitcoin-detached-sigs.git ~/bitcoin-detached-sigs
```

For each release, import the builder keys and check the tag:

```bash
gpg --import ~/guix.sigs/builder-keys/*.gpg
cd ~/bitcoin
git fetch --tags
git checkout $TAG
git verify-tag $TAG
```

## 4. Build (in tmux)

```bash
tmux new -s guix        # reattach later: tmux attach -t guix
```

```bash
cd ~/bitcoin
export JOBS=$(nproc)
export HOSTS="x86_64-linux-gnu arm-linux-gnueabihf aarch64-linux-gnu riscv64-linux-gnu powerpc64-linux-gnu x86_64-w64-mingw32"
./contrib/guix/guix-build
```

Notes:

* **Do not add `powerpc64le-linux-gnu`.** `guix-build` leaves it out of the
  default HOSTS on purpose ("disabled due to non-determinism issues across
  build arches"). Nobody else attests it, so including it adds two hashes no
  other builder can match.
* The HOSTS above skip darwin, which needs the Xcode SDK. To build macOS too:
  1. Generate the SDK tarball by following `contrib/macdeploy/README.md` (needs the Xcode `.xip`).
  2. Extract it with `mkdir -p ~/macOS-SDKs && tar -C ~/macOS-SDKs -xaf Xcode-*-extracted-SDK-with-libcxx-headers.tar.gz`.
  3. Add `x86_64-apple-darwin arm64-apple-darwin` to HOSTS (or unset HOSTS to use the defaults).
  4. Run with `SDK_PATH=~/macOS-SDKs`.
* Optional: add `SOURCES_PATH=~/depends-sources BASE_CACHE=~/depends-cache` to
  reuse depends downloads and builds between releases.
* Outputs go to `~/bitcoin/guix-build-$VERSION/output/<host>/`.

## 5. Attest the non-codesigned build (optional early PR)

You can stop here, attest, and PR `noncodesigned.SHA256SUMS` right away. You can
also wait for codesigning and do everything in one go in step 7.

```bash
env GUIX_SIGS_REPO=~/guix.sigs SIGNER=$SIGNER NO_SIGN=1 ./contrib/guix/guix-attest
ls ~/guix.sigs/$VERSION/$SIGNER/      # -> noncodesigned.SHA256SUMS
```

## 6. Codesigning

Release maintainers publish the detached signatures to `bitcoin-detached-sigs`.
This usually happens a day or so after the tag. First check that the tag
exists:

```bash
cd ~/bitcoin-detached-sigs
git fetch --tags
git tag -l "$TAG"        # empty output = not published yet, wait
git checkout $TAG
```

```bash
cd ~/bitcoin
# Windows only (matches the HOSTS above):
env DETACHED_SIGS_REPO=~/bitcoin-detached-sigs HOSTS="x86_64-w64-mingw32" ./contrib/guix/guix-codesign
# If darwin was built too, drop the HOSTS override (default = win + both macOS)
```

To confirm it worked:

```bash
ls ~/bitcoin/guix-build-$VERSION/output/x86_64-w64-mingw32-codesigned/
# -> bitcoin-$VERSION-win64-setup.exe, bitcoin-$VERSION-win64.zip, SHA256SUMS.part
```

## 7. Attest (no signing, server)

`guix-attest` won't overwrite an existing file whose contents differ, so remove
the old output first if you're re-attesting:

```bash
rm -rf ~/guix.sigs/$VERSION/$SIGNER/
cd ~/bitcoin
env GUIX_SIGS_REPO=~/guix.sigs SIGNER=$SIGNER NO_SIGN=1 ./contrib/guix/guix-attest
```

```bash
ls ~/guix.sigs/$VERSION/$SIGNER/
# -> all.SHA256SUMS  noncodesigned.SHA256SUMS
grep -E 'win64(-setup)?\.(exe|zip)|codesignatures' ~/guix.sigs/$VERSION/$SIGNER/all.SHA256SUMS
# should list 3 lines: win64-setup.exe, win64.zip, codesignatures-*.tar.gz
```

## 8. Verify against other builders (server)

```bash
git -C ~/guix.sigs fetch upstream
git -C ~/guix.sigs merge --ff-only upstream/main    # or: git pull
gpg --import ~/guix.sigs/builder-keys/*.gpg
env GUIX_SIGS_REPO=~/guix.sigs ./contrib/guix/guix-verify
```

`guix-verify` checks the signed attestations only, so my unsigned ones are
skipped. Use this to compare my hashes to everyone else's by filename:

```bash
cd ~/guix.sigs
for f in noncodesigned all; do
  for d in $VERSION/*/; do
    b=$(basename "$d"); [ "$b" = "$SIGNER" ] && continue
    [ -f "$d/$f.SHA256SUMS" ] || continue
    bad=$(awk 'NR==FNR{h[$2]=$1; next} ($2 in h) && h[$2]!=$1 {print $2}' \
          "$d/$f.SHA256SUMS" "$VERSION/$SIGNER/$f.SHA256SUMS")
    extra=$(awk 'NR==FNR{h[$2]=1; next} !($2 in h){print $2}' \
          "$d/$f.SHA256SUMS" "$VERSION/$SIGNER/$f.SHA256SUMS" | tr '\n' ' ')
    printf '%-18s %-14s %s %s\n' "$b" "$f" "${bad:+MISMATCH: $bad}${bad:-ok}" "${extra:+(only mine: $extra)}"
  done
done
```

Every line should say `ok`. If `only mine:` shows up for every builder, I built
a host nobody else builds (e.g. powerpc64le). Darwin files that only the others
have are fine.

## 9. Fork remotes (once)

```bash
cd ~/guix.sigs
git remote rename origin upstream
git remote add origin git@github.com:$SIGNER/guix.sigs.git
git remote -v
```

## 10. Push the unsigned attestation (server)

Always branch from a **fresh** `upstream/main`, or the PR will carry stale
history:

```bash
cd ~/guix.sigs
git fetch upstream
git checkout -B ${SIGNER}_$VERSION upstream/main
git add $VERSION/$SIGNER/
git commit -m "Add attestations by $SIGNER for $VERSION (unsigned, WIP)"
git push -u origin ${SIGNER}_$VERSION
```

## 11. Sign (laptop)

Sign with the key that's committed in `builder-keys/yuvicc.gpg`. That's
`40D97A0E5A4F40B1CC582522CA247AC78DF0509C` (*yuvicc-sign-release*), **not** my
everyday key. Check first:

```bash
gpg --list-secret-keys --keyid-format long
gpg --show-keys ~/guix.sigs/builder-keys/$SIGNER.gpg
```

```bash
cd ~/guix.sigs
git fetch origin
git checkout ${SIGNER}_$VERSION
git reset --hard origin/${SIGNER}_$VERSION
cd $VERSION/$SIGNER
for f in noncodesigned.SHA256SUMS all.SHA256SUMS; do
  gpg --local-user 40D97A0E5A4F40B1CC582522CA247AC78DF0509C \
      --detach-sign --digest-algo sha256 --armor --output "$f.asc" "$f"
done
```

```bash
gpg --verify noncodesigned.SHA256SUMS.asc noncodesigned.SHA256SUMS
gpg --verify all.SHA256SUMS.asc all.SHA256SUMS
```

## 12. Commit & open the PR (laptop)

```bash
cd ~/guix.sigs
git add $VERSION/$SIGNER/
git commit --amend -m "Add attestations by $SIGNER for $VERSION (all)"
git push --force-with-lease origin ${SIGNER}_$VERSION
gh pr create --repo bitcoin-core/guix.sigs --base main \
  --title "$SIGNER $VERSION all attestations" --body ""
```

Use `(noncodesigned)` or `(codesigned)` in the title if only one part is in the PR.

## 13. Clean up (server)

```bash
cd ~/bitcoin
./contrib/guix/guix-clean
```

This runs `git clean -xdff` on the bitcoin repo, which removes **every
untracked file**, including `guix-build-*/` with all its outputs. It shows a
dry run and asks before deleting. Only run it after the PR is in.

## Troubleshooting

* **"ERR: Build directories for this commit already exist"**: a previous run
  left `distsrc-*` dirs behind. Delete the ones it lists
  (`guix-build-$VERSION/distsrc-$VERSION-<host>`) or run `guix-clean`.
* **"ERR: An ... SHA256SUMS file already exists ... and attests differently"**:
  remove `~/guix.sigs/$VERSION/$SIGNER/` and run `guix-attest` again.
* **`all.SHA256SUMS` missing after attest**: the `*-codesigned` output dir doesn't
  exist. Run step 6.
* **Hash mismatch with other builders**: make sure `git describe` in `~/bitcoin`
  shows exactly `$TAG` with a clean tree (`git status`), then clean and rebuild
  that host.
