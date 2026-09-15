test_exe = executable(
    'commons_test',
    [
        'commons_test.c',
        '../src/commons.c',
    ],
    include_directories: include_directories('../src'),
    install: false,
)

test('commons', test_exe)
