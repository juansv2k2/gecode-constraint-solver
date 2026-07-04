#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PKG_DIR="$ROOT_DIR/max-package/gecode-solver"
BUILT_BUNDLE="$ROOT_DIR/bin/gecode.solver.mxo"

deploy_to_user_package() {
  local package_root="$1"
  local dest="$package_root/gecode-solver"
  local installed_bundle="$dest/externals/gecode.solver.mxo"
  local installed_bin="$installed_bundle/Contents/MacOS/gecode.solver"

  echo "[smoke] syncing full package to: $dest"
  rm -rf "$dest"
  cp -R "$PKG_DIR" "$dest"

  # Remove quarantine/extended attributes and ad-hoc sign the installed bundle.
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

  # Sync any weights files: datasets/weights/<name>.json -> examples/weights/<name>.json
  # Runs whenever a matching filename exists in both places, keeping the package
  # copy current without requiring a manual cp after each retrain.
  WEIGHTS_PKG_DIR="$PKG_DIR/examples/weights"
  WEIGHTS_SRC_DIR="$ROOT_DIR/datasets/weights"
  if [[ -d "$WEIGHTS_PKG_DIR" ]]; then
    synced=0
    for wf in "$WEIGHTS_PKG_DIR"/*.json; do
      [[ -f "$wf" ]] || continue
      fname="$(basename "$wf")"
      src="$WEIGHTS_SRC_DIR/$fname"
      if [[ -f "$src" ]]; then
        cp "$src" "$wf"
        echo "[smoke] synced weights: $fname"
        synced=$((synced + 1))
      fi
    done
    [[ $synced -eq 0 ]] && echo "[smoke] weights: no datasets/weights/ matches found for package weights"
  fi

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
