"""PlatformIO pre-build hook: apply local patches to managed library deps.

Why this exists:
  We need a NULL-guard fix in esp32_idf5_https_server's HTTPSConnection.cpp
  (see patches/esp32_idf5_https_server-null-guards.patch and the wedge story
  in tests/wedge_repro.sh). The library is installed under .pio/libdeps/, so
  edits there get blown away whenever PlatformIO refreshes deps. This script
  re-applies the patches on every build so the fix survives clean checkouts,
  CI, dep updates, etc.

How it works:
  - For each .patch under patches/, look up the target library directory
    under .pio/libdeps/<env>/<libname>/ (filename prefix before "-" is the
    lib name).
  - Try to apply via `patch -p1 --dry-run` first; if it succeeds, apply for
    real. If the dry-run reports "already applied", skip.
  - Fail the build if a patch can't be applied (better to fail loudly than
    silently ship un-patched code).

Wired in by `extra_scripts = pre:scripts/apply_patches.py` in platformio.ini.
"""

import os
import subprocess
import glob

Import("env")  # type: ignore  # provided by PlatformIO SCons

PROJECT_DIR    = env["PROJECT_DIR"]
LIBDEPS_DIR    = env.subst("$PROJECT_LIBDEPS_DIR")  # .pio/libdeps
ENV_NAME       = env["PIOENV"]
PATCHES_DIR    = os.path.join(PROJECT_DIR, "patches")


def lib_dir_for(patchfile):
    """Return the absolute path to the library dir this patch targets.

    Patch filename convention: <lib-name>-<short-description>.patch.
    E.g. esp32_idf5_https_server-null-guards.patch → lib dir
    .pio/libdeps/<env>/esp32_idf5_https_server.
    """
    name = os.path.basename(patchfile)
    libname = name.split("-", 1)[0]
    return os.path.join(LIBDEPS_DIR, ENV_NAME, libname)


def apply_patch(patchfile, libdir):
    """Apply one patch idempotently. Raises CalledProcessError on hard failure."""
    rel = os.path.relpath(patchfile, PROJECT_DIR)

    # Dry-run forward to see if it applies cleanly.
    dry = subprocess.run(
        ["patch", "-p1", "--dry-run", "-i", patchfile],
        cwd=libdir, capture_output=True, text=True,
    )
    if dry.returncode == 0:
        print(f"[apply_patches] applying {rel}")
        subprocess.run(
            ["patch", "-p1", "-i", patchfile],
            cwd=libdir, check=True,
        )
        return

    # Dry-run reverse: if it would *un*-apply cleanly, the patch is already in place.
    rev = subprocess.run(
        ["patch", "-p1", "-R", "--dry-run", "-i", patchfile],
        cwd=libdir, capture_output=True, text=True,
    )
    if rev.returncode == 0:
        print(f"[apply_patches] skipping {rel} (already applied)")
        return

    # Neither direction works — patch is broken or the target moved.
    print(f"[apply_patches] FAILED to apply {rel}:\n{dry.stdout}\n{dry.stderr}")
    raise RuntimeError(f"could not apply {rel}")


def main():
    if not os.path.isdir(PATCHES_DIR):
        return  # no patches to apply
    for patchfile in sorted(glob.glob(os.path.join(PATCHES_DIR, "*.patch"))):
        libdir = lib_dir_for(patchfile)
        if not os.path.isdir(libdir):
            print(f"[apply_patches] target lib dir not found for {patchfile} "
                  f"(looked at {libdir}); skipping — PlatformIO may not have "
                  f"installed it yet")
            continue
        apply_patch(patchfile, libdir)


main()
