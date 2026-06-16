import configparser
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def list_files():
    print("Project files:")
    for root, dirs, files in os.walk(ROOT):
        if root.startswith(os.path.join(ROOT, '.pio')):
            continue
        rel_root = os.path.relpath(root, ROOT)
        if rel_root == '.':
            rel_root = ''
        for filename in sorted(files):
            print(os.path.join(rel_root, filename).lstrip('./\\'))


def show_platformio():
    config_path = os.path.join(ROOT, 'platformio.ini')
    if not os.path.exists(config_path):
        print('platformio.ini not found')
        return
    config = configparser.ConfigParser()
    config.read(config_path)
    print('PlatformIO environments:')
    for section in config.sections():
        print(f'- {section}')
        for key, value in config[section].items():
            print(f'    {key} = {value}')


COMMANDS = {
    'list': list_files,
    'platformio': show_platformio,
}


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in COMMANDS:
        print('Usage: python agent/agent.py <command>')
        print('Commands: list, platformio')
        return
    COMMANDS[sys.argv[1]]()


if __name__ == '__main__':
    main()
