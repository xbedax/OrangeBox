# Project Agent

This is a lightweight local agent for the `box_initial` PlatformIO project.

## Purpose

- Inspect project structure
- List source and configuration files
- Provide a simple entry point for future project automation commands

## Usage

From the repository root:

```bash
python agent/agent.py list
```

## Commands

- `list`: show workspace sources and config files
- `platformio`: show parsed `platformio.ini` environment settings
