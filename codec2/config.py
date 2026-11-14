"""Validated server settings, with optional TOML and environment overrides."""

from dataclasses import dataclass, field, fields
import json
import os
from pathlib import Path
import secrets
import tomllib
from urllib.parse import urlsplit

from common import valid_token

DEFAULT_CONFIG = Path(__file__).resolve().parent / "config.toml"


@dataclass(frozen=True)
class Settings:
    publish_token: str = field(default="", repr=False)
    listen_token: str = field(default="", repr=False)
    tcp_host: str = "127.0.0.1"
    tcp_port: int = 7778
    http_host: str = "127.0.0.1"
    http_port: int = 8080
    viewer_origin: str = ""
    codec2_lib: str = ""
    max_tcp_connections: int = 16
    max_viewers: int = 8

    def validate(self):
        for name in ("publish_token", "listen_token"):
            if not valid_token(getattr(self, name)):
                raise ValueError(f"{name} must contain 32-128 URL-safe ASCII characters")
        if self.publish_token == self.listen_token:
            raise ValueError("Use different publishing and listening tokens")
        for name in ("tcp_host", "http_host"):
            value = getattr(self, name)
            if not isinstance(value, str) or not value or value.strip() != value:
                raise ValueError(f"{name} must be a nonempty hostname or IP address")
        for name in ("tcp_port", "http_port"):
            value = getattr(self, name)
            if type(value) is not int or not 1 <= value <= 65535:
                raise ValueError(f"{name} must be an integer between 1 and 65535")
        for name in ("max_tcp_connections", "max_viewers"):
            value = getattr(self, name)
            if type(value) is not int or not 1 <= value <= 128:
                raise ValueError(f"{name} must be an integer between 1 and 128")
        if not isinstance(self.codec2_lib, str) or not isinstance(self.viewer_origin, str):
            raise ValueError("codec2_lib and viewer_origin must be strings")
        if self.viewer_origin:
            origin = urlsplit(self.viewer_origin)
            if (origin.scheme not in ("http", "https") or not origin.hostname or
                    origin.username is not None or origin.password is not None or
                    origin.path or origin.query or origin.fragment):
                raise ValueError("viewer_origin must be an HTTP(S) origin without a path or trailing slash")
            origin.port  # Validate an explicitly supplied port as well.
        return self


ENV = {item.name: "C2_" + item.name.upper() for item in fields(Settings)}
ENV["codec2_lib"] = "CODEC2_LIB"


def load_settings(path=None, environ=None):
    environ = os.environ if environ is None else environ
    explicit_path = path is not None
    path = Path(path) if path is not None else DEFAULT_CONFIG
    values = {}
    if path.exists():
        with path.open("rb") as source:
            values = tomllib.load(source)
    elif explicit_path:
        raise ValueError(f"Configuration file does not exist: {path}")
    unknown = set(values) - set(ENV)
    if unknown:
        raise ValueError("Unknown configuration keys: " + ", ".join(sorted(unknown)))
    for name, variable in ENV.items():
        if variable in environ:
            values[name] = environ[variable]
            if name.endswith("_port") or name.startswith("max_"):
                try:
                    values[name] = int(values[name])
                except ValueError:
                    raise ValueError(f"{variable} must be an integer") from None
    return Settings(**values).validate()


def write_config(path):
    """Create new credentials without overwriting an existing configuration."""
    settings = Settings(publish_token=secrets.token_hex(32), listen_token=secrets.token_hex(32))
    lines = ["# N70 Codec 2 proxy. Keep this file private; do not commit it.",
             "# Environment variables override these values.", ""]
    for item in fields(Settings):
        lines.append(f"{item.name} = {json.dumps(getattr(settings, item.name))}")
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, "w", encoding="utf-8") as output:
        output.write("\n".join(lines) + "\n")
