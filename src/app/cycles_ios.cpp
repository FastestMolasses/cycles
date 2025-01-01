#include <stdio.h>

#include "device/device.h"
#include "scene/camera.h"
#include "scene/integrator.h"
#include "scene/scene.h"
#include "session/buffers.h"
#include "session/session.h"

#include "util/path.h"
#include "util/string.h"
#include "util/function.h"

#ifdef WITH_USD
#  include "hydra/file_reader.h"
#endif

#include "app/cycles_xml.h"
#include "app/oiio_output_driver.h"
#include "cycles_ios.h"

CCL_NAMESPACE_BEGIN

struct Options {
  Session *session;
  Scene *scene;
  string filepath;
  int width, height;
  SceneParams scene_params;
  SessionParams session_params;
  bool quiet;
  bool show_help, interactive, pause;
  string output_filepath;
  string output_pass;
} options;

void validate_options();
static void session_print(const string &str);
static void session_print_status();
static BufferParams &session_buffer_params();

void cycles_ios_initialize(const CyclesInitParams *params)
{
  validate_options();

  // Initialize path and logging
  util_logging_init(nullptr);
  path_init();

  // Set up options
  options.width = params->width;
  options.height = params->height;
  options.filepath = params->filepath;
  options.quiet = params->quiet;

  // Session params
  options.session_params.samples = params->samples;
  options.session_params.threads = params->threads;
  options.session_params.background = params->background;
  options.session_params.use_profiling = params->use_profiling;
  options.session_params.tile_size = params->tile_size;
  options.session_params.use_auto_tile = options.session_params.tile_size > 0;

  vector<DeviceInfo> devices = Device::available_devices(DEVICE_MASK(DEVICE_METAL));
  if (!devices.empty()) {
    options.session_params.device = devices.front();
  }
  else {
    fprintf(stderr, "No matching device found for: %s\n", params->device_name);
    return;
  }

  if (string(params->shading_system) == "osl") {
    options.scene_params.shadingsystem = SHADINGSYSTEM_OSL;
  }
  else {
    options.scene_params.shadingsystem = SHADINGSYSTEM_SVM;
  }

#ifdef WITH_OSL
  if (options.scene_params.shadingsystem == SHADINGSYSTEM_OSL &&
      options.session_params.device.type != DEVICE_CPU)
  {
    fprintf(stderr, "OSL shading system only works with CPU device\n");
    return;
  }
#endif

  options.output_pass = "combined";
  options.session = new Session(options.session_params, options.scene_params);

  if (!options.output_filepath.empty()) {
    options.session->set_output_driver(make_unique<OIIOOutputDriver>(
        options.output_filepath, options.output_pass, session_print));
  }

  if (options.session_params.background && !options.quiet) {
    options.session->progress.set_update_callback(function_bind(&session_print_status));
  }

  options.scene = options.session->scene;

#ifdef WITH_USD
  if (!string_endswith(string_to_lower(options.filepath), ".xml")) {
    HD_CYCLES_NS::HdCyclesFileReader::read(options.session, options.filepath.c_str());
  }
  else
#endif
  {
    xml_read_file(options.scene, options.filepath.c_str());
  }

  // Camera width/height override?
  if (!(options.width == 0 || options.height == 0)) {
    options.scene->camera->set_full_width(options.width);
    options.scene->camera->set_full_height(options.height);
  }
  else {
    options.width = options.scene->camera->get_full_width();
    options.height = options.scene->camera->get_full_height();
  }
  options.scene->camera->compute_auto_viewplane();

  Pass *pass = options.scene->create_node<Pass>();
  pass->set_name(ustring(options.output_pass.c_str()));
  pass->set_type(PASS_COMBINED);

  options.session->reset(options.session_params, session_buffer_params());
  options.session->start();
}

void cycles_ios_render()
{
  if (options.session) {
    options.session->wait();
  }
}

void cycles_ios_cleanup()
{
  if (options.session) {
    delete options.session;
    options.session = NULL;
  }
}

void validate_options()
{
  if (options.session_params.device.type == DEVICE_NONE) {
    fprintf(stderr, "Unknown device: %s\n", options.session_params.device.name.c_str());
    exit(EXIT_FAILURE);
  } else if (options.session_params.samples < 0) {
    fprintf(stderr, "Invalid number of samples: %d\n", options.session_params.samples);
    exit(EXIT_FAILURE);
  } else if (options.filepath == "") {
    fprintf(stderr, "No file path specified\n");
    exit(EXIT_FAILURE);
  }
}

static BufferParams &session_buffer_params()
{
  static BufferParams buffer_params;
  buffer_params.width = options.width;
  buffer_params.height = options.height;
  buffer_params.full_width = options.width;
  buffer_params.full_height = options.height;

  return buffer_params;
}

static void session_print_status()
{
  string status, substatus;

  double progress = options.session->progress.get_progress();
  options.session->progress.get_status(status, substatus);

  if (substatus != "") {
    status += ": " + substatus;
  }

  status = string_printf("Progress %05.2f   %s", (double)progress * 100, status.c_str());
  session_print(status);
}

static void session_print(const string &str)
{
  printf("\r%s", str.c_str());

  static int maxlen = 0;
  int len = str.size();
  maxlen = max(len, maxlen);

  for (int i = len; i < maxlen; i++) {
    printf(" ");
  }

  fflush(stdout);
}

CCL_NAMESPACE_END
