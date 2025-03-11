__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import pyslang
from intervaltree import IntervalTree


class PyslangRewriter:
    def __init__(self, syntax_tree: pyslang.SyntaxTree) -> None:
        self._syntax_tree = syntax_tree

        # Additions are a mapping of indices to list of strings, where the
        # strings are inserted before the index in order they appear.
        self._additions: dict[int, list[str]] = {}

        # Deletions are a list of [start:stop] tuples of indices of the original
        # source. Additions to deleted indices are also deleted.
        self._deletions: list[tuple[int, int]] = []

        # All additions and deletions must happen on the same buffer ID.
        self._buffer_id: pyslang.BufferID | None = None

    def add_before(self, location: pyslang.SourceLocation, text: str) -> None:
        self._set_or_check_buffer_id(location.buffer)
        self._additions.setdefault(location.offset, []).append(text)

    def remove(self, range_: pyslang.SourceRange) -> None:
        self._set_or_check_buffer_id(range_.start.buffer)
        self._set_or_check_buffer_id(range_.end.buffer)
        assert range_.start.offset <= range_.end.offset
        self._deletions.append((range_.start.offset, range_.end.offset))

    def _set_or_check_buffer_id(self, buffer_id: pyslang.BufferID) -> None:
        if self._buffer_id is None:
            self._buffer_id = buffer_id
        else:
            assert self._buffer_id == buffer_id

    def commit(self) -> pyslang.SyntaxTree:
        if self._buffer_id is None:
            return self._syntax_tree

        # If text is not null-terminated, appending to the end won't work.
        text = self._syntax_tree.sourceManager.getSourceText(self._buffer_id)
        assert ord(text[-1]) == 0, "original source must be null-terminated"

        # Break original source into intervals based on additions and deletions.
        intervals = IntervalTree.from_tuples([(0, len(text))])
        for index in self._additions:
            intervals.slice(index)
        for start, stop in self._deletions:
            intervals.chop(start, stop)

        # Assemble new source text from the intervals and added text.
        pieces = []
        for interval in sorted(intervals):
            start, stop, _ = interval
            pieces.extend(self._additions.get(start, []))
            pieces.append(text[start:stop])

        self._syntax_tree = pyslang.SyntaxTree.fromText("".join(pieces))
        self._buffer_id = None
        self._additions.clear()
        self._deletions.clear()
        return self._syntax_tree
