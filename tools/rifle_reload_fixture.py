"""Live installed Bullpup magazine calibration; no invented action geometry."""
from insertion_fixture import make_fixture as _make_fixture

def make_fixture(pid, game, folder):
    return _make_fixture(pid, game, folder, weapon='bullpup')
