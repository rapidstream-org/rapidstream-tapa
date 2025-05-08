"""Suggest and compress an identifier to a shorter name if it is too long."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
from itertools import count

MAX_NAME_LENGTH = 64

_logger = logging.getLogger(__name__)


def suggest_name(name: str, blocklist: set[str]) -> str:
    r"""Suggest a name that is not in the namespace.

    Args:
        name (str): The name to suggest.
        blocklist (set[str]): A set of names that are not available.

    Returns:
        str: The suggested name.

    Examples:
        >>> suggest_name("foo", {"foo"})
        'foo_1'

        >>> suggest_name("\\foo- ", {"\\foo- "})
        '\\foo-_1 '
    """
    if len(name) > MAX_NAME_LENGTH:
        original_name = name
        name = compress_name(name, MAX_NAME_LENGTH)
        _logger.debug('Name "%s" is compressed to "%s".', original_name, name)

    if name not in blocklist:
        return name

    # compress the name further to make room for the suffix
    if len(name) > MAX_NAME_LENGTH - 16:
        short_name = compress_name(name, MAX_NAME_LENGTH - 16)
    else:
        short_name = name

    for suffix in count(1):
        if (
            new_name := concat_identifiers(short_name, "_", str(suffix))
        ) not in blocklist:
            _logger.debug('"%s" is replaced with name "%s" instead.', name, new_name)
            return new_name

    msg = "Should not reach here"
    raise NotImplementedError(msg)


def compress_name(name: str, target_len: int) -> str:
    """Compress an identifier to a shorter name.

    Args:
        name: The name to compress.
        target_len: The target length of the compressed name.

    Returns:
        The compressed name.

    Examples:
        If the name is already shorter than the target length, the name is
        returned unchanged.

        >>> compress_name("foo", 4)
        'foo'

        If the name has multiple components, and some of the components have already
        existed in the name before this component, the longest component will be
        removed until the name is shorter than the target length.

        >>> compress_name("foo_bar_foo_foo", 11)
        'foo_bar_foo'
        >>> compress_name("foo_bar_foo_foo", 10)
        'foo_bar'

        If some of the components are a subsequence of other components, only the
        longest component will be kept until the name is shorter than the target length.
        The components with a longer subsequences will be removed first.

        >>> compress_name("foo_bar_fooo_barrr", 14)
        'bar_fooo_barrr'

        >>> compress_name("foo_bar_fooo_barrr", 10)
        'fooo_barrr'

        If, still, the name is longer than the target length, the longest component will
        be removed until the name is shorter than the target length.

        >>> compress_name("foo_bar_tackle_toe", 11)
        'foo_bar_toe'

        After all the process, if the name is still longer than the target length, the
        name will be truncated from the left.

        >>> compress_name("foooo", 3)
        'foo'
    """
    # If the name is already shorter than the target length, return the name.
    if len(name) <= target_len:
        return name

    # Get the components of the name.
    components = name.split("_")

    # First, iterate through the components and remove the components that are
    # a subsequence of other components, from the longest to the shortest.
    while get_current_length(components) > target_len:
        # If no subsequence is found, break
        if not is_removed_longest_subcomponent(components):
            break

    # Second, iterate through the components and remove the longest component.
    while get_current_length(components) > target_len and len(components) > 1:
        components.pop(components.index(max(components, key=len)))

    # If the name is still longer truncate the name from the left.
    name = "_".join(components)
    if len(name) > target_len:
        name = name[:target_len]

    # the first character must not be a number
    if name[0].isdigit():
        name = "_" + name[:-1]

    return name


def is_removed_longest_subcomponent(components: list[str]) -> bool:
    """Remove the longest subcomponent from the components.

    Args:
        components: The components of the name.

    Returns:
        True if a subsequence is found and removed, False otherwise.

    Examples:
        >>> components = ["foo", "bar", "fooo", "barrr"]
        >>> is_removed_longest_subcomponent(components)
        True
        >>> components
        ['bar', 'fooo', 'barrr']
    """
    component_to_keep = ""
    subsequence_to_remove = ""
    subsequence_index = -1

    # For each pair of components, find the longest subsequence.
    for index_comp, comp in enumerate(components):
        for index_sub, comp_sub in reversed(list(enumerate(components))):
            # If comp_sub is a subsequence of comp or itself.
            if index_comp == index_sub or comp_sub not in comp:
                continue

            diff_current = len(comp) - len(comp_sub)
            diff_previous = (
                len(component_to_keep) - len(subsequence_to_remove)
                if subsequence_index != -1
                # If no subsequence is found, set the previous difference to infinity.
                else float("inf")
            )

            if (
                # First priority: the shortest subsequence difference
                diff_current < diff_previous
                # Second priority: the longest component
                or (
                    diff_current == diff_previous
                    and len(comp_sub) > len(subsequence_to_remove)
                )
            ):
                component_to_keep = comp
                subsequence_to_remove = comp_sub
                subsequence_index = index_sub

    # If no subsequence is found
    if subsequence_index == -1:
        return False

    # Remove the subsequence from the components.
    components.pop(subsequence_index)
    return True


def get_current_length(components: list[str]) -> int:
    """Get the current length of the name from the components."""
    return sum(len(component) for component in components) + len(components) - 1


def concat_identifiers(*identifiers: str) -> str:
    r"""Return the concatenated identifiers.

    Args:
        identifiers (list[str]): The list of identifiers to concatenate.

    Returns:
        str: The concatenated identifiers.

    Examples:
        >>> concat_identifiers("a", "b", "c")
        'abc'
        >>> concat_identifiers("a", "b", "c", "d")
        'abcd'
        >>> concat_identifiers("\\a- ", "b", "c")
        '\\a-bc '
    """
    unescaped_identifiers = [unescape_identifier(ident) for ident in identifiers]
    return escape_identifier("".join(unescaped_identifiers))


def unescape_identifier(ident: str) -> str:
    r"""Return the unescaped identifier.

    Args:
        ident (str): The identifier to unescape.

    Returns:
        str: The unescaped identifier.

    Examples:
        >>> unescape_identifier("test")
        'test'
        >>> unescape_identifier("test_")
        'test_'
        >>> unescape_identifier("\\test- ")
        'test-'
        >>> unescape_identifier("\\test-1 ")
        'test-1'
    """
    if ident[0] == "\\":
        return ident[1:].strip()
    return ident


def escape_identifier(ident: str) -> str:
    r"""Return the escaped identifier.

    Args:
        ident (str): The identifier to escape.

    Returns:
        str: The escaped identifier.

    Examples:
        >>> escape_identifier("test")
        'test'
        >>> escape_identifier("test_")
        'test_'
        >>> escape_identifier("test-")
        '\\test- '
        >>> escape_identifier("test-1")
        '\\test-1 '
    """
    for c in ident:
        if not c.isalnum() and c != "_":
            return f"\\{ident} "
    return ident
