#!/usr/bin/env python3

import argparse
import hashlib
import logging
import os
import subprocess
import sys
import textwrap
from typing import List


VERSION = '0.0.1'


def create_argparser() -> argparse.ArgumentParser:
    description = 'Replace inline PlantUML content with images'
    epilog = textwrap.dedent(f'''\
        Intended usage:
        mkdir -p generated
        rm generated/*
        {sys.argv[0]} *.md
        ''')
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=description, epilog=epilog)
    parser.add_argument('-V', type=int, dest='verbosity', default=1,
                        help='set verbosity')
    parser.add_argument('-v', '--version',
                        action='version', version=f'%(prog)s {VERSION}',
                        help='show version and exit')
    parser.add_argument('files', metavar='file.md', type=str, nargs='+',
                        help='input file (Markdown)')
    return parser


def configure_logging(args: argparse.Namespace) -> None:
    if args.verbosity == 0:
        log_level = logging.WARN
    elif args.verbosity == 1:
        log_level = logging.INFO
    else:
        log_level = logging.DEBUG
    logging.basicConfig(format='%(levelname)s:%(message)s', level=log_level)


def convert_files(args: argparse.Namespace, output_dir: str) -> None:
    for ifile in args.files:
        convert_file(ifile, output_dir)


def convert_file(filename: str, output_dir: str) -> None:
    logging.info(f'Converting {filename}')
    with open(filename, 'r') as ifile:
        in_puml = False
        puml_lines: List[str] = []
        output_lines = []
        for lineno, line in enumerate(ifile, 1):
            line = line.rstrip('\n')
            if line.startswith('```plantuml'):
                if in_puml:
                    logging.error(f'{filename}:{lineno}: nested plantuml')
                else:
                    in_puml = True
                    puml_lines = []
            elif in_puml and line.startswith('```'):
                img_filename = convert_puml(filename, output_dir, puml_lines)
                output_lines.append(f'![]({img_filename})')
                in_puml = False
            elif in_puml:
                puml_lines.append(line)
            else:
                output_lines.append(line)
        write_outputfile(filename, output_dir, output_lines)


# TODO Do all conversions in one java call
def convert_puml(filename: str, output_dir: str, puml_lines: List[str]) -> str:
    puml_text = '\n'.join(puml_lines) + '\n'
    puml_bytes = puml_text.encode(encoding='utf-8', errors='replace')
    key = hashlib.sha1(puml_bytes).hexdigest()
    puml_filename = key + '.puml'
    puml_path = os.path.join(output_dir, puml_filename)
    logging.debug(f'Writing {puml_path}')
    write_outputfile(puml_filename, output_dir, puml_lines)
    cmd = ['java', '-jar', 'plantuml.jar', '-tsvg', puml_path]
    logging.debug(f'Calling {" ".join(cmd)}')
    subprocess.run(cmd, check=True)
    logging.debug(f'Removing {puml_path}')
    os.remove(puml_path)
    img_filename = key + '.svg'
    return img_filename


def write_outputfile(filename: str, output_dir: str, lines: List[str]) -> None:
    path = os.path.join(output_dir, filename)
    logging.info(f'Writing {path}')
    with open(path, 'w') as ofile:
        for line in lines:
            ofile.write(line + '\n')


def main() -> None:
    parser = create_argparser()
    args = parser.parse_args()
    configure_logging(args)
    convert_files(args, 'generated')


main()
