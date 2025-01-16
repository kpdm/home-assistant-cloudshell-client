project('hacli', 'c')

dep_ncurses = dependency('ncurses')
dep_panel = dependency('panel')
dep_mosquitto = dependency('libmosquitto')
dep_json = dependency('jansson')
dep_thread = dependency('threads')

src = ['main.c', 'hacsc_ui.c']

executable('hacli-x', src, dependencies: [dep_ncurses, dep_panel, dep_mosquitto, dep_json, dep_thread])


