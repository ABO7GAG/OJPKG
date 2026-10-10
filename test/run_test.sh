#!/bin/sh
# End-to-end smoke test for ojpkg. Builds two throwaway test packages
# (ojbase: no deps + install hooks; ojbar: depends on ojbase) and exercises
# search / install / query / remove, including dependency resolution and
# the "refuse to remove a depended-upon package" safety check.
set -e
cd "$(dirname "$0")"
OJPKG=../ojpkg
[ -x "$OJPKG" ] || { echo "Build ojpkg first: (cd .. && make)"; exit 1; }

rm -rf pkgbuild-a pkgbuild-b repo testroot
mkdir -p pkgbuild-a/usr/bin pkgbuild-b/etc/ojbar repo testroot
ROOT="$(pwd)/testroot"

# --- package: ojbase (no deps, has install hooks) ---
cat > pkgbuild-a/.PKGINFO <<'EOF'
pkgname = ojbase
pkgver = 1.0.0-1
pkgdesc = Base utilities for OJLinux
arch = x86_64
size = 20
EOF
printf '#!/bin/sh\necho "hello from ojbase"\n' > pkgbuild-a/usr/bin/ojbase-hello
chmod +x pkgbuild-a/usr/bin/ojbase-hello
cat > pkgbuild-a/.INSTALL <<'EOF'
pre_install() { echo "[hook] about to install ojbase"; }
post_install() { echo "[hook] ojbase installed!"; }
EOF
tar -czf repo/ojbase-1.0.0-1.ojpkg -C pkgbuild-a .PKGINFO .INSTALL usr

# --- package: ojbar (depends on ojbase) ---
cat > pkgbuild-b/.PKGINFO <<'EOF'
pkgname = ojbar
pkgver = 0.3.1-2
pkgdesc = Example config-carrying package
arch = x86_64
size = 5
depend = ojbase
EOF
echo "color=blue" > pkgbuild-b/etc/ojbar/config
tar -czf repo/ojbar-0.3.1-2.ojpkg -C pkgbuild-b .PKGINFO etc

echo "== search =="
$OJPKG --repo repo search oj

echo; echo "== install ojbar (must auto-install ojbase first) =="
$OJPKG --root "$ROOT" --repo repo install ojbar

echo; echo "== query (installed list) =="
$OJPKG --root "$ROOT" query

echo; echo "== query ojbase (info) =="
$OJPKG --root "$ROOT" query ojbase

echo; echo "== filesystem check =="
find "$ROOT" -type f

echo; echo "== re-install ojbase directly (should say already installed) =="
$OJPKG --root "$ROOT" install repo/ojbase-1.0.0-1.ojpkg

echo; echo "== try removing ojbase while ojbar depends on it (must refuse) =="
if $OJPKG --root "$ROOT" remove ojbase; then
    echo "FAIL: should have refused"; exit 1
else
    echo "OK: correctly refused"
fi

echo; echo "== remove ojbar then ojbase (must succeed) =="
$OJPKG --root "$ROOT" remove ojbar
$OJPKG --root "$ROOT" remove ojbase

echo; echo "== final query (should be empty) =="
$OJPKG --root "$ROOT" query
echo "ALL TESTS PASSED"
