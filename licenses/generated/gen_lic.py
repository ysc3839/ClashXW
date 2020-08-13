import os

rc = open('LicensesText.rc', 'w', newline='\n')
hpp = open('LicensesList.hpp', 'w', newline='\n')

rc.write(r'1 TEXT "..\\..\\LICENSE"')
rc.write('\n')

hpp.write('''#pragma once

constexpr std::array licenseList = {
''');

i = 2
for f in os.scandir('..'):
    if not f.is_file():
        continue
    path = f.path.replace("\\", "\\\\")
    rc.write(f'{i} TEXT "{path}"\n')
    hpp.write(f'\tL"{f.name}",\n')
    i += 1

hpp.write('''};
''');
