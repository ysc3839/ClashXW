import os

rc = open('LicensesText.rc', 'w', newline='\n')
hpp = open('LicensesList.hpp', 'w', newline='\n')

hpp.write('''#pragma once

constexpr std::array licenseList = {
''');

i = 1
for f in os.scandir('..'):
    if not f.is_file():
        continue
    rc.write(f'{i} TEXT "{f.path}"\n')
    hpp.write(f'\tL"{f.name}",\n')
    i += 1

hpp.write('''};
''');
