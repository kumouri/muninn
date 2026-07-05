"""Write transcripts to Markdown (canonical) — one file per capture segment."""

from __future__ import annotations

from datetime import datetime
from pathlib import Path

from .whisper_runner import TranscriptSegment


def transcript_name(when: datetime) -> str:
    return when.strftime("%Y-%m-%d_%H%M%S") + ".md"


def _body(segments: list[TranscriptSegment], labels: list[str] | None) -> str:
    lines: list[str] = []
    for i, seg in enumerate(segments):
        text = seg.text.strip()
        if not text:
            continue
        if labels is not None and i < len(labels):
            lines.append(f"**{labels[i]}:** {text}")
        else:
            lines.append(text)
    # Labeled transcripts read as a speaker-per-line script; plain ones as one paragraph.
    return "\n\n".join(lines) if labels is not None else " ".join(lines)


def write_transcript(
    out_dir: str | Path,
    segments: list[TranscriptSegment],
    *,
    labels: list[str] | None = None,
    when: datetime | None = None,
    duration_s: float = 0.0,
    source: str = "muninn",
) -> Path:
    """Write one transcript and return its path. `when` defaults to now (injectable for tests)."""
    when = when or datetime.now()
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    path = out / transcript_name(when)

    header = (
        f"# Transcript — {when:%Y-%m-%d %H:%M:%S}\n\n"
        f"- **Source:** {source}\n"
        f"- **Duration:** {duration_s:.1f}s\n"
    )
    if labels is not None:
        header += "- **Diarized:** You / Program (by input channel)\n"
    body = header + "\n---\n\n" + _body(segments, labels) + "\n"

    path.write_text(body, encoding="utf-8")
    return path
