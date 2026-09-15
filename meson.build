project(
    'pidload',
    ['c', 'cpp'],
    version: run_command(
        'sh',
        '-c', '''
        v=$(
            command -v zfr >/dev/null 2>&1 && zfr version 2>/dev/null || true
        )
        if [ -z "$v" ] && [ -f VERSION ]; then
            v=$(head -n1 VERSION | tr -d '\r')
            v=${v#v}
        fi
        if [ -z "$v" ]; then
            v="0.0.0" # FIXED TO 0.0.0, DO NOT MODIFY
        fi
        printf '%s' "$v"
        ''',
        check: false,
    ).stdout().strip(),
    license: 'AGPL-3.0-or-later',
    default_options: [
        'c_std=c99',
        'cpp_std=c++17',
        'warning_level=2',
    ],
)

project_author = 'Lenik'
project_email = 'pidload@bodz.net'
project_year = 2026

asciidoctor = find_program('asciidoctor', required: true)
fs = import('fs')

prefix = get_option('prefix')
bindir = prefix / get_option('bindir')
datadir = prefix / get_option('datadir')
mandir = prefix / get_option('mandir')
localedir = prefix / get_option('localedir')
pkgdocdir = datadir / 'doc' / meson.project_name()

config_h = configuration_data()
config_h.set_quoted('PROJECT_VERSION', meson.project_version())
config_h.set_quoted('PROJECT_AUTHOR', project_author)
config_h.set_quoted('PROJECT_EMAIL', project_email)
config_h.set('PROJECT_YEAR', project_year)
config_h.set_quoted('LOCALEDIR', localedir)

configure_file(
    output: 'config.h',
    configuration: config_h,
)

bas_c_dep = dependency('bas-c', required: true)
bas_args = ['-DLOGGER_NAME=' + meson.project_name(), '-DTEXT_DOMAIN=' + meson.project_name()]

wx_dep = dependency('wxwidgets', version: '>=3.0', required: true, modules: ['std', 'aui'])
x11_dep = dependency('x11', required: true)

inc = include_directories('src', '.')

subdir('po')

pidload_sources = [
    'src/pidload.cpp',
    'src/options.cpp',
    'src/util.cpp',
    'src/process_match.cpp',
    'src/collector.cpp',
    'src/recorder.cpp',
    'src/chart_panel.cpp',
    'src/main_frame.cpp',
    'src/dialogs.cpp',
]

executable(
    'pidload',
    pidload_sources,
    include_directories: inc,
    dependencies: [bas_c_dep, wx_dep, x11_dep],
    cpp_args: bas_args,
    install: true,
    install_dir: bindir,
)

bash_files = [
    'pidload.bash',
]

foreach file : bash_files
    name = fs.stem(file)
    install_data(
        file,
        install_dir: datadir / 'bash-completion' / 'completions',
        rename: name,
    )
endforeach

install_data(
    [
        'LICENSE',
        'README.md',
        'README-zh_CN.md',
    ],
    install_dir: pkgdocdir,
)

custom_target(
    'pidload-man',
    input: 'man/pidload.adoc',
    output: 'pidload.1',
    command: [
        asciidoctor,
        '-b', 'manpage',
        '-a', 'project-version=' + meson.project_version(),
        '-a', 'project-year=@0@'.format(project_year),
        '-a', 'project-author=' + project_author,
        '-a', 'project-email=' + project_email,
        '-o', '@OUTPUT@',
        '@INPUT@',
    ],
    build_by_default: true,
    install: true,
    install_dir: mandir / 'man1',
)

manpage_langs = ['ar', 'de', 'es_MX', 'fr', 'it', 'ja', 'ko', 'zh_CN']
foreach lang : manpage_langs
    custom_target(
        'pidload-man-' + lang,
        input: 'man' / lang / 'pidload.adoc',
        output: 'pidload.1.' + lang,
        command: [
            asciidoctor,
            '-b', 'manpage',
            '-a', 'project-version=' + meson.project_version(),
            '-a', 'project-year=@0@'.format(project_year),
            '-a', 'project-author=' + project_author,
            '-a', 'project-email=' + project_email,
            '-o', '@OUTPUT@',
            '@INPUT@',
        ],
        build_by_default: true,
    )
endforeach

meson.add_install_script(
    'sh',
    '-euc',
    '''
        build="@0@"
        mandir="@1@"
        for lang in ar de es_MX fr it ja ko zh_CN; do
            dest="${DESTDIR}${mandir}/${lang}/man1"
            mkdir -p "$dest"
            install -m644 "$build/pidload.1.$lang" "$dest/pidload.1"
        done
    '''.format(meson.project_build_root(), mandir),
)

run_target(
    'posync',
    command: [
        'bash',
        '-eu',
        '-c',
        'cd "$1/po" && xgettext --from-code=UTF-8 --keyword=_ --keyword=tr --language=C++ --directory=.. --output=pidload.pot --files-from=POTFILES --package-name=pidload --package-version="$2" --no-wrap && while IFS= read -r lang; do [ -n "$lang" ] || continue; case "$lang" in "#"* ) continue ;; esac; po_file="$lang.po"; if [ ! -f "$po_file" ]; then msginit --no-translator --no-wrap --input=pidload.pot --locale="$lang" --output-file="$po_file"; fi; msgmerge --update --backup=none --no-wrap "$po_file" pidload.pot; msgattrib --no-obsolete --no-wrap --output-file="$po_file" "$po_file"; done < LINGUAS',
        'posync',
        meson.project_source_root(),
        meson.project_version(),
    ],
)

subdir('tests')

run_target(
    'look',
    command: [
        'bash',
        '-euc', '''
            tmpdir=$(mktemp -d)
            DESTDIR=$tmpdir meson install -C "@BUILD_ROOT@"
            tree "$tmpdir"
            rm -fr "$tmpdir"
        ''',
    ],
)

run_target(
    'install-symlinks',
    command: [
        'bash',
        '-euc', '''
            prefix="@0@"
            bindir="@1@"
            datadir="@2@"
            mandir="@3@"
            mkdir -p "$bindir" "$datadir/bash-completion/completions" "$mandir/man1"
            sudo ln -sfn "@BUILD_ROOT@/pidload" "$bindir/pidload"
            sudo ln -sfn "@BUILD_ROOT@/pidload.1" "$mandir/man1/pidload.1"
            sudo ln -sfn "@SOURCE_ROOT@/pidload.bash" "$datadir/bash-completion/completions/pidload"
            printf "Symlinks installed under %s\n" "$prefix"
        '''.format(get_option('prefix'), bindir, datadir, mandir),
    ],
)

run_target(
    'uninstall-symlinks',
    command: [
        'bash',
        '-euc', '''
            for p in \
                "@1@/pidload" \
                "@3@/man1/pidload.1" \
                "@2@/bash-completion/completions/pidload"
            do
                if [ -L "$p" ]; then
                    sudo rm -f "$p"
                    printf "Removed %s\n" "$p"
                else
                    printf "Skipped (not a symlink): %s\n" "$p"
                fi
            done
        '''.format(get_option('prefix'), bindir, datadir, mandir),
    ],
)
