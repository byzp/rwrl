import configparser
import sys
from pathlib import Path

_python_dir = str(Path(__file__).resolve().parent.parent / 'py')
if _python_dir not in sys.path:
    sys.path.insert(0, _python_dir)
from resource_paths import SERVER_CONFIG_FILE, TRAIN_CONFIG_FILE, project_path

_DEFAULTS = {
    'server': {
        'host': '0.0.0.0',
        'port': '5123',
        'max_players': '12',
        'speed': '1.0',
        'starting_credits': '4000',
        'income_mult': '1.0',
        'unit_cap': '250',
        'map_width': '2000',
        'map_height': '2000',
    },
    'game': {
        'tick_rate': '60',
        'sync_interval': '60',
        'game_over_delay': '5.0',
    },
    'rl': {
        'enabled': 'false',
        'tick_rate': '0',
    },
    'ai': {
        'model_path': 'models/ice_strategy_v3/bc_best.pt',
        'device': 'auto',
        'deterministic': 'true',
        'ticks_per_inference': '10',
    },
    'diagnostics': {
        'enabled': 'false',
        'log_file': 'logs/server_packets.jsonl',
    },
    'admin': {
        'password': 'admin',
        'enable_http': 'true',
        'http_port': '8080',
    },
}


def load_train_config(path: str | Path | None = None) -> dict:
    """Read TOML training settings relative to the project root."""
    try:
        import tomllib
    except ModuleNotFoundError:  # Python 3.10
        import tomli as tomllib
    with project_path(path or TRAIN_CONFIG_FILE).open('rb') as stream:
        return tomllib.load(stream)


class Config:
    def __init__(self, path=None):
        self._cp = configparser.ConfigParser()
        for section, values in _DEFAULTS.items():
            self._cp[section] = dict(values)
        with SERVER_CONFIG_FILE.open(encoding='utf-8') as stream:
            self._cp.read_file(stream)
        if path:
            with project_path(path).open(encoding='utf-8') as stream:
                self._cp.read_file(stream)

    def get(self, section, key, fallback=None):
        return self._cp.get(section, key, fallback=fallback)

    def getint(self, section, key, fallback=0):
        return self._cp.getint(section, key, fallback=fallback)

    def getfloat(self, section, key, fallback=0.0):
        return self._cp.getfloat(section, key, fallback=fallback)

    def getbool(self, section, key, fallback=False):
        return self._cp.getboolean(section, key, fallback=fallback)

    @property
    def host(self): return self.get('server', 'host')
    @property
    def port(self): return self.getint('server', 'port')
    @property
    def max_players(self): return self.getint('server', 'max_players')
    @property
    def speed(self): return self.getfloat('server', 'speed')
    @property
    def starting_credits(self): return self.getfloat('server', 'starting_credits')
    @starting_credits.setter
    def starting_credits(self, value): self._cp.set('server', 'starting_credits', str(value))
    @property
    def income_mult(self): return self.getfloat('server', 'income_mult')
    @income_mult.setter
    def income_mult(self, value): self._cp.set('server', 'income_mult', str(value))
    @property
    def unit_cap(self): return self.getint('server', 'unit_cap')
    @property
    def map_width(self): return self.getint('server', 'map_width')
    @property
    def map_height(self): return self.getint('server', 'map_height')
    @property
    def tick_rate(self): return self.getint('game', 'tick_rate')
    @property
    def sync_interval(self): return self.getint('game', 'sync_interval')

    @property
    def rl_enabled(self): return self.getbool('rl', 'enabled')

    @property
    def rl_tick_rate(self): return self.getint('rl', 'tick_rate')

    @property
    def ai_model_path(self): return self.get('ai', 'model_path')

    @property
    def ai_device(self): return self.get('ai', 'device')

    @property
    def ai_deterministic(self): return self.getbool('ai', 'deterministic')

    @property
    def ai_ticks_per_inference(self):
        return max(1, self.getint('ai', 'ticks_per_inference', fallback=10))

    @property
    def diagnostics_enabled(self):
        return self.getbool('diagnostics', 'enabled', fallback=False)

    @property
    def diagnostics_log_file(self):
        return self.get('diagnostics', 'log_file', fallback='logs/server_packets.jsonl')
