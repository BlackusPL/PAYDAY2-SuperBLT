import argparse
import base64
import time
import zipfile
from pathlib import Path

from cryptography.hazmat.primitives.asymmetric.mldsa import MLDSA65PrivateKey
from cryptography.hazmat.primitives import serialization

DIR = Path(__file__).parent

README_FILES = [
    'README.txt',
    'epic-step-1.png',
    'epic-step-2.png',
]


def add_file(data: bytes, name: str, zf: zipfile.ZipFile, hidden=False):
    info = zipfile.ZipInfo(name, date_time=time.localtime(time.time())[:6])

    if hidden:
        # Mark the file as hidden
        info.create_system = 0  # 0 = MS-DOS/Windows, so external_attr is read as DOS attrs
        info.external_attr = 0x02  # FILE_ATTRIBUTE_HIDDEN

    zf.writestr(info, data)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dll-path', required=True)
    parser.add_argument('--out', required=True)
    parser.add_argument('--key-path', required=True)

    args = parser.parse_args()

    out_file = Path(args.out)

    if out_file.exists():
        out_file.unlink()

    with open(args.dll_path, 'rb') as fi:
        dll_data = fi.read()

    # 256 random bytes, per 3.6.1 of FIPS 204
    # Generate with base64.standard_b64encode(secrets.token_bytes(32))
    # You can use testkey-private-seed.txt for development purposes.
    with open(args.key_path, 'r') as fi:
        private_seed = base64.urlsafe_b64decode(fi.read().strip())
    private_key = MLDSA65PrivateKey.from_seed_bytes(private_seed)

    # Uncomment to get the string for updater_pubkey.h
    public_bytes = private_key.public_key().public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw
    )
    c_key = ", ".join([f'0x{b:02x}' for b in public_bytes])
    # print(c_key)

    sig = private_key.sign(dll_data, context=b'')

    with zipfile.ZipFile(out_file, 'x') as zf:
        add_file(dll_data, 'WSOCK32.dll', zf)
        add_file(sig, 'WSOCK32.dll.sig', zf, hidden=True)

        for name in README_FILES:
            with open(DIR / name, 'rb') as fi:
                add_file(fi.read(), name, zf)

    pass


if __name__ == '__main__':
    main()
