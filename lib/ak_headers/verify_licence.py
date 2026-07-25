from pathlib import Path
from typing import List
import sys
import os

DUAL_HEADER = """
/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the
"Apache License"); you may not use this file except in compliance with the
Apache License. You may obtain a copy of the Apache License at
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

Copyright (c) 2026 Audiokinetic Inc.
*******************************************************************************/
""".strip()

DIR = Path(__file__).parent


def check_license_headers(directory: Path, valid_headers: List[str]) -> None:
    count = 0

    files = sorted(directory.rglob('*'))
    for file in files:
        if not file.is_file():
            continue

        count += 1

        content = file.read_text()

        # Some of the licence headers have trailing spaces on their lines, strip those
        # (this also removes the leading spaces on the copyright line)
        lines = content.splitlines()
        for i in range(len(lines)):
            lines[i] = lines[i].strip()
        content = '\n'.join(lines)

        if not any(content.startswith(h) for h in valid_headers):
            print(f"BAD LICENSE HEADER: {file}")

            # print(content)
            # sys.exit(0)

            # file.unlink()

    print(f"Searched {count} files.")


def main():
    check_license_headers(DIR / 'include', [DUAL_HEADER])


if __name__ == '__main__':
    main()
