"""Write transcripts to Markdown (canonical) — one file per capture segment."""

from __future__ import annotations

from datetime import datetime
from pathlib import Path


def transcript_name(when: datetime) -> str:
    return when.strftime("%Y-%m-%d_%H%M%S") + ".md"


def write_transcript(
    out_dir: str | Path,
    text: str,
    *,
    when: datetime | None = None,
    duration_s: float = 0.0,
    source: str = "muninn",
) -> Path:
    """Write one transcript and return its path. `when` defaults to now (injectable for tests)."""
    when = when or datetime.now()
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    path = out / transcript_name(when)

    body = (
        f"# Transcript — {when:%Y-%m-%d %H:%M:%S}\n\n"
        f"- **Source:** {source}\n"
        f"- **Duration:** {duration_s:.1f}s\n\n"
        f"---\n\n"
        f"{text.strip()}\n"
    )
    path.write_text(body, encoding="utf-8")
    return path
