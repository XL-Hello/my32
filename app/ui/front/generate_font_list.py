#!/usr/bin/env python3
"""将控件文案中的字符按首次出现顺序去重，生成 LVGL 字体字符列表。"""

from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
RAW_LIST_PATH = SCRIPT_DIR / "list_raw.txt"
FONT_LIST_PATH = SCRIPT_DIR / "list.txt"
LINE_SEPARATORS = {"\r", "\n"}


def unique_characters(text: str) -> str:
    """移除换行并按首次出现顺序保留每个 Unicode 字符。"""
    seen: set[str] = set()
    characters: list[str] = []

    for character in text:
        if character in LINE_SEPARATORS or character in seen:
            continue
        seen.add(character)
        characters.append(character)

    return "".join(characters)


def main() -> None:
    raw_text = RAW_LIST_PATH.read_text(encoding="utf-8")
    font_characters = unique_characters(raw_text)

    if len(font_characters) != len(set(font_characters)):
        raise RuntimeError("生成的字体字符列表包含重复字符")

    FONT_LIST_PATH.write_text(font_characters, encoding="utf-8")
    print(
        f"已生成 {FONT_LIST_PATH}: "
        f"原始字符 {len(raw_text)} 个，去重后字符 {len(font_characters)} 个"
    )


if __name__ == "__main__":
    main()
