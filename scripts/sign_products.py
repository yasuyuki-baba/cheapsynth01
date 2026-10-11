#!/usr/bin/env python3
"""Opt-in signing preparation for built products; never loads a plugin.

Default is a dry run. Identity names/thumbprints refer to preconfigured stores;
no private key, certificate password or credential file is read by this script.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys

MAC_BUNDLES = {'.app', '.vst3', '.component', '.clap'}
MACHO_MAGIC = {b'\xfe\xed\xfa\xce', b'\xce\xfa\xed\xfe',
               b'\xfe\xed\xfa\xcf', b'\xcf\xfa\xed\xfe',
               b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca',
               b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca'}


def signing_commands(root, platform, identity):
    root = root.resolve()
    if not root.is_dir():
        raise ValueError('Build output directory does not exist')
    if not identity.strip() or identity.startswith('-'):
        raise ValueError('Provide a signing identity name or certificate thumbprint')
    if platform == 'Windows':
        if len(identity) != 40 or any(c not in '0123456789abcdefABCDEF' for c in identity):
            raise ValueError('Windows identity must be a 40-digit SHA1 certificate thumbprint')
        products = sorted(p for p in root.rglob('*') if p.is_file() and p.suffix.lower() in {'.exe', '.vst3', '.clap'})
        if not products:
            raise ValueError('No Windows products found')
        commands = []
        for product in products:
            if product.is_symlink() or not product.resolve().is_relative_to(root):
                raise ValueError('Product escapes the output directory')
            commands.extend([
                ['signtool', 'sign', '/sha1', identity, '/fd', 'SHA256', '/tr', 'https://timestamp.digicert.com', '/td', 'SHA256', str(product)],
                ['signtool', 'verify', '/pa', '/all', str(product)],
            ])
        return commands
    if platform != 'macOS':
        raise ValueError('Supported signing platforms are Windows and macOS')
    products = sorted(p for p in root.iterdir() if p.is_dir() and p.suffix in MAC_BUNDLES)
    if not products:
        raise ValueError('No macOS products found')
    commands = []
    for product in products:
        if product.is_symlink():
            raise ValueError('Top-level product cannot be a symlink')
        # Sign nested executable code before its enclosing product. Do not use
        # codesign --deep to sign: verify uses --deep to inspect all nested code.
        nested = []
        for path in product.rglob('*'):
            if path.is_symlink():
                if not path.resolve().is_relative_to(product.resolve()):
                    raise ValueError('Bundle symlink escapes its product')
                continue
            if path.is_dir() and path.suffix in MAC_BUNDLES | {'.framework', '.bundle'}:
                nested.append(path)
            elif path.is_file():
                with path.open('rb') as stream:
                    if stream.read(4) in MACHO_MAGIC:
                        nested.append(path)
        for path in sorted(nested, key=lambda p: (-len(p.parts), str(p))) + [product]:
            commands.append(['codesign', '--force', '--options', 'runtime', '--timestamp', '--sign', identity, str(path)])
        commands.append(['codesign', '--verify', '--deep', '--strict', '--verbose=2', str(product)])
    return commands


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True, help='Native build out directory')
    parser.add_argument('--platform', choices=['Windows', 'macOS'], required=True)
    parser.add_argument('--identity', required=True, help='Installed certificate thumbprint or keychain identity name')
    parser.add_argument('--execute', action='store_true', help='Sign products; default only prints commands')
    args = parser.parse_args()
    commands = signing_commands(args.output, args.platform, args.identity)
    if args.execute:
        if (args.platform == 'Windows') != (sys.platform == 'win32') or (args.platform == 'macOS' and sys.platform != 'darwin'):
            parser.error('Execute signing on its native platform')
        for command in commands:
            subprocess.run(command, check=True)
    else:
        print(json.dumps(commands, indent=2))


if __name__ == '__main__':
    main()
