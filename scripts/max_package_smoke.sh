#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PKG_DIR="$ROOT_DIR/max-package/gecode-solver"
BUILT_BUNDLE="$ROOT_DIR/bin/gecode.solver.mxo"

deploy_to_user_package() {
  local package_root="$1"
  local target_dir="$package_root/gecode-solver/externals"

  mkdir -p "$target_dir"
  rm -rf "$target_dir/gecode.solver.mxo"
  cp -R "$BUILT_BUNDLE" "$target_dir/"

  local installed_bundle="$target_dir/gecode.solver.mxo"
  local installed_bin="$installed_bundle/Contents/MacOS/gecode.solver"

  # Remove quarantine/extended attributes and ad-hoc sign the whole bundle.
  xattr -cr "$installed_bundle" || true
  if command -v codesign >/dev/null 2>&1; then
    codesign --force --deep --sign - "$installed_bundle"
  fi

  if command -v otool >/dev/null 2>&1; then
    if otool -L "$installed_bin" | grep -q "MaxAPIImpl"; then
      echo "[smoke] warning: deployed bundle still depends on MaxAPIImpl at: $installed_bin"
    else
      echo "[smoke] deployed bundle verified at: $installed_bin"
    fi
  fi
}

echo "[smoke] checking package structure..."
for d in "$PKG_DIR" "$PKG_DIR/externals" "$PKG_DIR/help" "$PKG_DIR/examples" "$PKG_DIR/docs"; do
  if [[ ! -d "$d" ]]; then
    echo "[smoke] missing directory: $d"
    exit 1
  fi
done

for f in \
  "$PKG_DIR/package-info.json" \
  "$PKG_DIR/help/gecode.solver.maxhelp" \
  "$PKG_DIR/examples/demo_config.json" \
  "$PKG_DIR/docs/README_MAX_PACKAGE.txt"
do
  if [[ ! -f "$f" ]]; then
    echo "[smoke] missing file: $f"
    exit 1
  fi
done

echo "[smoke] package metadata and assets present"

echo "[smoke] building Max external (make max-external)..."
make -C "$ROOT_DIR" max-external

if [[ -d "$BUILT_BUNDLE" ]]; then
  rm -rf "$PKG_DIR/externals/gecode.solver.mxo"
  cp -R "$BUILT_BUNDLE" "$PKG_DIR/externals/"
  echo "[smoke] copied built external bundle into package externals/"

  # Also deploy to active user package folders so Max loads the latest build.
  if [[ -d "$HOME/Documents/Max 8/Packages" ]]; then
    deploy_to_user_package "$HOME/Documents/Max 8/Packages"
  fi
  if [[ -d "$HOME/Documents/Max 9/Packages" ]]; then
    deploy_to_user_package "$HOME/Documents/Max 9/Packages"
  fi
else
  echo "[smoke] note: bin/gecode.solver.mxo not found (build with make max-external MAX_SDK_PATH=... )"
fi

echo "[smoke] done"
