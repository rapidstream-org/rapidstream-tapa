from tapa.common.base import Base


class Constant(Base):
  """TAPA constant passed as an argument to a task."""

  def __init__(self, *args, **kwargs):
    """Constructs a TAPA constant."""
    super(Constant, self).__init__(*args, **kwargs)
    self.global_name = self.name  # constant's name is global