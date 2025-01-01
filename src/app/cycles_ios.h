#ifdef __cplusplus
extern "C" {
#endif

struct CyclesInitParams {
  int width;
  int height;
  const char *filepath;
  int samples;
  int threads;
  const char *shading_system;  // "svm" or "osl"
  bool use_auto_tile;
  int tile_size;
  bool background;
  bool quiet;
  bool use_profiling;
};

void cycles_ios_initialize(const CyclesInitParams *params);
void cycles_ios_render();
void cycles_ios_cleanup();

#ifdef __cplusplus
}
#endif
