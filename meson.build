project('micropolis', 'c',
  version: '0.1.0',
  default_options: ['c_std=c99', 'warning_level=2'])

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)
sdl2_dep = dependency('sdl2', required: false)

inc_dir = include_directories(
  '../../src/core',
  '../../src/platform',
  '../../src/render',
  '../../src/sound',
  '../../src/libs',
  'src/micropolis/headers'
)

engine_sources = files(
  '../../src/core/alloc.c',
  '../../src/core/utils.c',
  '../../src/core/easing.c',
  '../../src/core/render.c',
  '../../src/core/image.c',
  '../../src/core/animation.c',
  '../../src/core/font.c',
  '../../src/core/sound.c',
  '../../src/core/map.c',
  '../../src/core/trace.c',
  '../../src/core/entity.c',
  '../../src/core/camera.c',
  '../../src/core/scene.c',
  '../../src/core/dither.c',
  '../../src/core/json.c',
  '../../src/core/noise.c',
  '../../src/core/input.c',
  '../../src/core/polygon.c',
  '../../src/core/svg.c',
  '../../src/core/file.c',
  '../../src/libs/mod4play/ft_tables.c',
  '../../src/libs/mod4play/m4p.c',
  '../../src/libs/mod4play/pmp_mix.c',
  '../../src/libs/mod4play/pmp_main.c',
  '../../src/libs/mod4play/pmplay.c',
  '../../src/libs/mod4play/snd_masm.c',
  '../../src/render/render_sdl.c',
  '../../src/platform/platform_sdl.c',
  '../../src/sound/sound_sdl.c'
)

deps = [m_dep]
if sdl2_dep.found()
  deps += sdl2_dep
endif

engine_lib = static_library('engine',
  engine_sources,
  include_directories: inc_dir,
  dependencies: deps
)

micropolis_sources = files(
  'src/micropolis/g_ani.c',
  'src/micropolis/rand.c',
  'src/micropolis/random.c',
  'src/micropolis/s_alloc.c',
  'src/micropolis/s_disast.c',
  'src/micropolis/s_eval.c',
  'src/micropolis/s_fileio.c',
  'src/micropolis/s_gen.c',
  'src/micropolis/s_init.c',
  'src/micropolis/s_msg.c',
  'src/micropolis/s_power.c',
  'src/micropolis/s_scan.c',
  'src/micropolis/s_sim.c',
  'src/micropolis/s_traf.c',
  'src/micropolis/s_zone.c',
  'src/micropolis/sim_stubs.c',
  'src/micropolis/w_budget.c',
  'src/micropolis/w_con.c',
  'src/micropolis/w_eval.c',
  'src/micropolis/w_keys.c',
  'src/micropolis/w_resrc.c',
  'src/micropolis/w_sound.c',
  'src/micropolis/w_sprite.c',
  'src/micropolis/w_tool.c',
  'src/micropolis/w_update.c',
  'src/micropolis/w_util.c'
)

micropolis_lib = static_library('micropolis_sim',
  micropolis_sources,
  include_directories: inc_dir,
  c_args: ['-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-sign-compare']
)

executable('micropolis',
  ['src/game.c', 'src/sim_render.c', 'src/sim_input.c', 'src/scene_title.c'],
  include_directories: inc_dir,
  link_with: [engine_lib, micropolis_lib],
  dependencies: deps
)


test_sim = executable('test_sim',
  ['src/test_sim.c'],
  include_directories: inc_dir,
  link_with: [engine_lib, micropolis_lib],
  dependencies: deps
)

test('micropolis_sim_test', test_sim)

