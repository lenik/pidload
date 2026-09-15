util_test = executable(
    'util_test',
    [
        'util_test.cpp',
        '../src/util.cpp',
    ],
    include_directories: include_directories('../src'),
    install: false,
)

test('util', util_test)

process_match_test = executable(
    'process_match_test',
    [
        'process_match_test.cpp',
        '../src/process_match.cpp',
        '../src/util.cpp',
    ],
    include_directories: include_directories('../src'),
    dependencies: [dependency('x11'), dependency('bas-c')],
    cpp_args: ['-DLOGGER_NAME=pidload'],
    install: false,
)

test('process_match', process_match_test)
