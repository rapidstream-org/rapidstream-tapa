__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""


class AXI:
    def __init__(self, name: str, data_width: int, addr_width: int) -> None:
        self.name = name
        self.data_width = data_width
        self.addr_width = addr_width


MAX_AXI_BRAM_ADDR_WIDTH = 24
