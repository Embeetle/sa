# Copyright © 2018-2026 Johan Cockx
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# SPDX-License-Identifier: GPL-3.0-or-later


class LineOffsetTable:
    def __init__(self, data):
        self.offsets = []
        total = 0
        for n in [ len(x) for x in data.split(b'\n') ]:
            self.offsets.append(total)
            total += n+1
        self.offsets.append(total-1)

    def line_count(self):
        return len(self.offsets)-1

    def line_length(self, line):
        if line < self.line_count():
            return self.offsets[line+1] - self.offsets[line]
        else:
            return 0

    def in_file(self, line, column):
        return line < self.line_count() and column < self.line_length(line)

    def offset(self, line, column):
        if line < self.line_count():
            return self.offsets[line] + column
        else:
            return self.offsets[-1]

if __name__ == '__main__':
    print('Test LineOffsetTable ...')

    table = LineOffsetTable(b'foo\nbar zot\ntail')

    print(f'{table.offsets}')
    assert table.offset(1,2) == 6

    with open(__file__, mode='rb') as file:
        data = file.read()
        table = LineOffsetTable(data)

    lines = data.split(b'\n')
    for n,line in enumerate(lines):
        print(f'{n} {table.offset(n,0)}: {line}')
        for i,c in enumerate(line):
            if data[table.offset(n,i)] != line[i]:
                print(f'{n}: {line}')
                assert False
