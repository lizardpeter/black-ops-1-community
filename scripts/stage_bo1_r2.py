"""Stage English BO1 assets on the R2 runner for a local Nuketown match."""
import argparse
from contextlib import ExitStack
from pathlib import Path, PurePosixPath
import shutil
import sys
from urllib.parse import quote
import zipfile


def game_path(name):
    parts = PurePosixPath(name.replace("\\", "/")).parts
    if any(part in ("..", "/") or ":" in part for part in parts):
        return None
    lower = tuple(part.lower() for part in parts)
    if lower[-1:] == ("localization.txt",):
        return Path("localization.txt")
    for root in ("main", "zone", "players", "miles"):
        if root not in lower:
            continue
        path = parts[lower.index(root):]
        if len(path) < 2:
            return None
        filename = path[-1].lower()
        if root == "main" and filename.startswith("localized_") and not filename.startswith("localized_english"):
            return None
        if root == "zone":
            if len(path) != 3 or path[1].lower() not in ("common", "english"):
                return None
            if not filename.endswith(".ff") or (filename.startswith("mp_") and not filename.startswith("mp_nuked.")):
                return None
        if Path(filename).suffix in {".dll", ".exe", ".bat", ".cmd", ".ps1", ".vbs", ".lnk"}:
            return None
        return Path(*path)
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runners", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(args.runners / "scripts"))
    from bo1_full_game_range import BO1_ZIP_KEY, EXPECTED_SIZE, HttpRangeObject, R2_BASES
    from r2_download import download

    args.destination.mkdir(parents=True, exist_ok=True)
    with ExitStack() as cleanup:
        archive = None
        for base in R2_BASES:
            try:
                source = cleanup.enter_context(HttpRangeObject(base + quote(BO1_ZIP_KEY, safe="/"), EXPECTED_SIZE))
                archive = cleanup.enter_context(zipfile.ZipFile(source))
                break
            except (OSError, RuntimeError, zipfile.BadZipFile) as error:
                print(f"R2 range read failed for {base}: {error}", flush=True)
        if archive is None:
            if shutil.disk_usage(args.destination).free < EXPECTED_SIZE + 8 * 1024**3:
                raise RuntimeError("Insufficient temporary disk for full R2 archive fallback")
            zip_path = args.destination.parent / "bo1-retail.zip"
            cleanup.callback(zip_path.unlink, missing_ok=True)
            size, _, _ = download(R2_BASES[0] + BO1_ZIP_KEY, zip_path, EXPECTED_SIZE)
            if size != EXPECTED_SIZE:
                raise RuntimeError("R2 archive size mismatch")
            archive = cleanup.enter_context(zipfile.ZipFile(zip_path))

        selected = {}
        for entry in archive.infolist():
            if entry.is_dir() or not (path := game_path(entry.filename)):
                continue
            key = path.as_posix().lower()
            if key in selected:
                raise RuntimeError(f"Duplicate game asset: {path}")
            selected[key] = (path, entry)

        required = {"localization.txt", "zone/common/common_mp.ff", "zone/common/mp_nuked.ff"}
        if missing := required - selected.keys():
            raise RuntimeError(f"Missing required BO1 assets: {sorted(missing)}")
        total = sum(entry.file_size for _, entry in selected.values())
        if total + 512 * 1024**2 > shutil.disk_usage(args.destination).free:
            raise RuntimeError(f"Insufficient disk for {total} bytes of game assets")

        for path, entry in sorted(selected.values(), key=lambda item: item[1].header_offset):
            output = args.destination / path
            output.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(entry) as source, output.open("wb") as destination:
                shutil.copyfileobj(source, destination, 1024**2)
        print(f"Staged {len(selected)} files ({total} bytes), including Nuketown.", flush=True)


if __name__ == "__main__":
    main()
