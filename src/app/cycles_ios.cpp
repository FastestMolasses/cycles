#include "device/device.h"
#include "scene/camera.h"
#include "scene/integrator.h"
#include "scene/scene.h"
#include "session/buffers.h"
#include "session/session.h"

#include "util/path.h"
#include "util/string.h"

#ifdef WITH_USD
#  include "hydra/file_reader.h"
#endif

#include "app/cycles_xml.h"
#include "app/oiio_output_driver.h"
#include "cycles_ios.h"

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

void cycles_ios_initialize(const CyclesInitParams *params)
{
  // Initialize path and logging
  util_logging_init(nullptr);
  path_init();

  // Set up options
  options.width = params->width;
  options.height = params->height;
  options.filepath = params->filepath;

  // Initialize the session
  options.output_pass = "combined";
  options.session = new Session(options.session_params, options.scene_params);

  if (!options.output_filepath.empty()) {
    options.session->set_output_driver(make_unique<OIIOOutputDriver>(
        options.output_filepath, options.output_pass, session_print));
  }

  if (options.session_params.background && !options.quiet) {
    options.session->progress.set_update_callback(function_bind(&session_print_status));
  }

  /* load scene */
  options.scene = options.session->scene;

  /* Read XML or USD */
#ifdef WITH_USD
  if (!string_endswith(string_to_lower(options.filepath), ".xml")) {
    HD_CYCLES_NS::HdCyclesFileReader::read(options.session, options.filepath.c_str());
  }
  else
#endif
  {
    xml_read_file(options.scene, options.filepath.c_str());
  }

  /* Camera width/height override? */
  if (!(options.width == 0 || options.height == 0)) {
    options.scene->camera->set_full_width(options.width);
    options.scene->camera->set_full_height(options.height);
  }
  else {
    options.width = options.scene->camera->get_full_width();
    options.height = options.scene->camera->get_full_height();
  }

  /* Calculate Viewplane */
  options.scene->camera->compute_auto_viewplane();

  /* add pass for output. */
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

  /* get status */
  double progress = options.session->progress.get_progress();
  options.session->progress.get_status(status, substatus);

  if (substatus != "") {
    status += ": " + substatus;
  }

  /* print status */
  status = string_printf("Progress %05.2f   %s", (double)progress * 100, status.c_str());
  session_print(status);
}

static void session_print(const string &str)
{
  /* print with carriage return to overwrite previous */
  printf("\r%s", str.c_str());

  /* add spaces to overwrite longer previous print */
  static int maxlen = 0;
  int len = str.size();
  maxlen = max(len, maxlen);

  for (int i = len; i < maxlen; i++) {
    printf(" ");
  }

  /* flush because we don't write an end of line */
  fflush(stdout);
}
