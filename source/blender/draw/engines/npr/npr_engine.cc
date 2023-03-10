/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_object.h"
#include "BKE_report.h"
#include "DEG_depsgraph_query.h"
#include "DNA_camera_types.h"
#include "DRW_render.h"
#include "ED_view3d.h"
#include "GPU_capabilities.h"
#include "draw_manager.hh"
#include "draw_pass.hh"

#include "npr_engine.h" /* Own include. */

namespace blender::NPR {

using namespace draw;

class Instance {
 public:
  View view = {"DefaultView"};
  int2 resolution_;

  PassMain prepass_ps_ = {"PrePass"};
  GPUShader *prepass_sh_ = nullptr;

  Texture id_tx_ = {"PrePass.ID"};
  Texture normal_tx_ = {"PrePass.Normal"};
  Texture tangent_tx_ = {"PrePass.Tangent"};
  Texture depth_tx_ = {"PrePass.Depth"};

  /* Line Width stored in red channel */
  Texture line_data_tx_ = {"PrePass.Line Data"};
  Texture line_color_tx_ = {"PrePass.Line Color"};

  Framebuffer prepass_fb_ = {"PrePass"};

  PassSimple deferred_pass_ps_ = {"Deferred Pass"};
  GPUShader *deferred_pass_sh_ = nullptr;

  Framebuffer deferred_pass_fb_ = {"Deferred Pass"};

  ~Instance()
  {
    DRW_SHADER_FREE_SAFE(prepass_sh_);
    DRW_SHADER_FREE_SAFE(deferred_pass_sh_);
  }

  void init(Object *camera_ob = nullptr)
  {
    GPUTexture *viewport_tx = DRW_viewport_texture_list_get()->color;
    resolution_ = int2(GPU_texture_width(viewport_tx), GPU_texture_height(viewport_tx));

    if (prepass_sh_ == nullptr) {
      prepass_sh_ = GPU_shader_create_from_info_name("npr_prepass_mesh");
      deferred_pass_sh_ = GPU_shader_create_from_info_name("npr_deferred_pass");
    }
  }

  void begin_sync()
  {
    eGPUTextureUsage usage_fb_tx = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;

    id_tx_.ensure_2d(GPU_R32UI, resolution_, usage_fb_tx);
    normal_tx_.ensure_2d(GPU_RGB16F, resolution_, usage_fb_tx);
    tangent_tx_.ensure_2d(GPU_RGBA16F, resolution_, usage_fb_tx);
    depth_tx_.ensure_2d(GPU_DEPTH24_STENCIL8, resolution_, usage_fb_tx);

    line_data_tx_.ensure_2d(GPU_R16F, resolution_, usage_fb_tx);
    line_color_tx_.ensure_2d(GPU_RGBA16F, resolution_, usage_fb_tx);

    prepass_ps_.init();
    prepass_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
    prepass_ps_.clear_color_depth_stencil(float4(0), 1.0f, 0);
    prepass_ps_.shader_set(prepass_sh_);

    deferred_pass_ps_.init();
    deferred_pass_ps_.state_set(DRW_STATE_WRITE_COLOR);
    deferred_pass_ps_.shader_set(deferred_pass_sh_);
    deferred_pass_ps_.bind_texture("id_tx", &id_tx_);
    deferred_pass_ps_.bind_texture("normal_tx", &normal_tx_);
    deferred_pass_ps_.bind_texture("depth_tx", &depth_tx_);
    deferred_pass_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }

  void end_sync()
  {
  }

  void object_sync(Manager &manager, ObjectRef &ob_ref)
  {
    Object *ob = ob_ref.object;
    if (!DRW_object_is_renderable(ob)) {
      return;
    }
    if (!(DRW_object_visibility_in_active_context(ob) & OB_VISIBLE_SELF)) {
      return;
    }
    if ((ob->dt < OB_SOLID) && !DRW_state_is_scene_render()) {
      return;
    }

    if (ob->type == OB_MESH) {
      ResourceHandle handle = manager.resource_handle(ob_ref);
      GPUBatch *batch = DRW_cache_object_surface_get(ob_ref.object);

      prepass_ps_.draw(batch, handle);
    }
  }

  void draw(Manager &manager, GPUTexture *depth_tx, GPUTexture *color_tx)
  {
    view.sync(DRW_view_default_get());

    prepass_fb_.ensure(GPU_ATTACHMENT_TEXTURE(depth_tx_),
                       GPU_ATTACHMENT_TEXTURE(id_tx_),
                       GPU_ATTACHMENT_TEXTURE(normal_tx_),
                       GPU_ATTACHMENT_TEXTURE(tangent_tx_));
    prepass_fb_.bind();

    manager.submit(prepass_ps_, view);

    deferred_pass_fb_.ensure(GPU_ATTACHMENT_NONE,
                             GPU_ATTACHMENT_TEXTURE(color_tx),
                             GPU_ATTACHMENT_TEXTURE(line_color_tx_),
                             GPU_ATTACHMENT_TEXTURE(line_data_tx_));
    deferred_pass_fb_.bind();

    manager.submit(deferred_pass_ps_);

    GPU_texture_copy(depth_tx, depth_tx_);
  }

  void draw_viewport(Manager &manager, GPUTexture *depth_tx, GPUTexture *color_tx)
  {
    this->draw(manager, depth_tx, color_tx);
  }
};

}  // namespace blender::NPR

/* -------------------------------------------------------------------- */
/** \name Interface with legacy C DRW manager
 * \{ */

using namespace blender;

struct NPR_Data {
  DrawEngineType *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  DRWViewportEmptyList *psl;
  DRWViewportEmptyList *stl;
  NPR::Instance *instance;

  char info[GPU_INFO_SIZE];
};

static void npr_engine_init(void *vedata)
{
  /* TODO(fclem): Remove once it is minimum required. */
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }

  NPR_Data *ved = reinterpret_cast<NPR_Data *>(vedata);
  if (ved->instance == nullptr) {
    ved->instance = new NPR::Instance();
  }

  ved->instance->init();
}

static void npr_cache_init(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  reinterpret_cast<NPR_Data *>(vedata)->instance->begin_sync();
}

static void npr_cache_populate(void *vedata, Object *object)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  draw::Manager *manager = DRW_manager_get();

  draw::ObjectRef ref;
  ref.object = object;
  ref.dupli_object = DRW_object_get_dupli(object);
  ref.dupli_parent = DRW_object_get_dupli_parent(object);

  reinterpret_cast<NPR_Data *>(vedata)->instance->object_sync(*manager, ref);
}

static void npr_cache_finish(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  reinterpret_cast<NPR_Data *>(vedata)->instance->end_sync();
}

static void npr_draw_scene(void *vedata)
{
  NPR_Data *ved = reinterpret_cast<NPR_Data *>(vedata);
  if (!GPU_shader_storage_buffer_objects_support()) {
    STRNCPY(ved->info, "Error: No shader storage buffer support");
    return;
  }
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  draw::Manager *manager = DRW_manager_get();
  ved->instance->draw_viewport(*manager, dtxl->depth, dtxl->color);
}

static void npr_instance_free(void *instance)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  delete reinterpret_cast<NPR::Instance *>(instance);
}

static void npr_view_update(void *vedata)
{
  NPR_Data *ved = reinterpret_cast<NPR_Data *>(vedata);
}

static void npr_id_update(void *vedata, struct ID *id)
{
  UNUSED_VARS(vedata, id);
}

/* RENDER */

static bool npr_render_framebuffers_init(void)
{
  /* For image render, allocate own buffers because we don't have a viewport. */
  const float2 viewport_size = DRW_viewport_size_get();
  const int2 size = {int(viewport_size.x), int(viewport_size.y)};

  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  /* When doing a multi view rendering the first view will allocate the buffers
   * the other views will reuse these buffers */
  if (dtxl->color == nullptr) {
    BLI_assert(dtxl->depth == nullptr);
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL;
    dtxl->color = GPU_texture_create_2d(
        "txl.color", size.x, size.y, 1, GPU_RGBA16F, usage, nullptr);
    dtxl->depth = GPU_texture_create_2d(
        "txl.depth", size.x, size.y, 1, GPU_DEPTH24_STENCIL8, usage, nullptr);
  }

  if (!(dtxl->depth && dtxl->color)) {
    return false;
  }

  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  GPU_framebuffer_ensure_config(
      &dfbl->default_fb,
      {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_TEXTURE(dtxl->color)});

  GPU_framebuffer_ensure_config(&dfbl->depth_only_fb,
                                {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_NONE});

  GPU_framebuffer_ensure_config(&dfbl->color_only_fb,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(dtxl->color)});

  return GPU_framebuffer_check_valid(dfbl->default_fb, nullptr) &&
         GPU_framebuffer_check_valid(dfbl->color_only_fb, nullptr) &&
         GPU_framebuffer_check_valid(dfbl->depth_only_fb, nullptr);
}

#ifdef DEBUG
/* This is just to ease GPU debugging when the frame delimiter is set to Finish */
#  define GPU_FINISH_DELIMITER() GPU_finish()
#else
#  define GPU_FINISH_DELIMITER()
#endif

static void write_render_color_output(struct RenderLayer *layer,
                                      const char *viewname,
                                      GPUFrameBuffer *fb,
                                      const struct rcti *rect)
{
  RenderPass *rp = RE_pass_find_by_name(layer, RE_PASSNAME_COMBINED, viewname);
  if (rp) {
    GPU_framebuffer_bind(fb);
    GPU_framebuffer_read_color(fb,
                               rect->xmin,
                               rect->ymin,
                               BLI_rcti_size_x(rect),
                               BLI_rcti_size_y(rect),
                               4,
                               0,
                               GPU_DATA_FLOAT,
                               rp->rect);
  }
}

static void write_render_z_output(struct RenderLayer *layer,
                                  const char *viewname,
                                  GPUFrameBuffer *fb,
                                  const struct rcti *rect,
                                  float4x4 winmat)
{
  RenderPass *rp = RE_pass_find_by_name(layer, RE_PASSNAME_Z, viewname);
  if (rp) {
    GPU_framebuffer_bind(fb);
    GPU_framebuffer_read_depth(fb,
                               rect->xmin,
                               rect->ymin,
                               BLI_rcti_size_x(rect),
                               BLI_rcti_size_y(rect),
                               GPU_DATA_FLOAT,
                               rp->rect);

    int pix_num = BLI_rcti_size_x(rect) * BLI_rcti_size_y(rect);

    /* Convert GPU depth [0..1] to view Z [near..far] */
    if (DRW_view_is_persp_get(nullptr)) {
      for (float &z : MutableSpan(rp->rect, pix_num)) {
        if (z == 1.0f) {
          z = 1e10f; /* Background */
        }
        else {
          z = z * 2.0f - 1.0f;
          z = winmat[3][2] / (z + winmat[2][2]);
        }
      }
    }
    else {
      /* Keep in mind, near and far distance are negatives. */
      float near = DRW_view_near_distance_get(nullptr);
      float far = DRW_view_far_distance_get(nullptr);
      float range = fabsf(far - near);

      for (float &z : MutableSpan(rp->rect, pix_num)) {
        if (z == 1.0f) {
          z = 1e10f; /* Background */
        }
        else {
          z = z * range - near;
        }
      }
    }
  }
}

static void npr_render_to_image(void *vedata,
                                struct RenderEngine *engine,
                                struct RenderLayer *layer,
                                const struct rcti *rect)
{
  /* TODO(fclem): Remove once it is minimum required. */
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }

  if (!npr_render_framebuffers_init()) {
    RE_engine_report(engine, RPT_ERROR, "Failed to allocate GPU buffers");
    return;
  }

  GPU_FINISH_DELIMITER();

  /* Setup */

  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Depsgraph *depsgraph = draw_ctx->depsgraph;

  NPR_Data *ved = reinterpret_cast<NPR_Data *>(vedata);
  if (ved->instance == nullptr) {
    ved->instance = new NPR::Instance();
  }

  /* TODO(sergey): Shall render hold pointer to an evaluated camera instead? */
  Object *camera_ob = DEG_get_evaluated_object(depsgraph, RE_GetCamera(engine->re));

  /* Set the perspective, view and window matrix. */
  float4x4 winmat, viewmat, viewinv;
  RE_GetCameraWindow(engine->re, camera_ob, winmat.ptr());
  RE_GetCameraModelMatrix(engine->re, camera_ob, viewinv.ptr());
  viewmat = math::invert(viewinv);

  DRWView *view = DRW_view_create(viewmat.ptr(), winmat.ptr(), nullptr, nullptr, nullptr);
  DRW_view_default_set(view);
  DRW_view_set_active(view);

  /* Render */
  do {
    if (RE_engine_test_break(engine)) {
      break;
    }

    ved->instance->init(camera_ob);

    DRW_manager_get()->begin_sync();

    npr_cache_init(vedata);
    auto npr_render_cache = [](void *vedata,
                               struct Object *ob,
                               struct RenderEngine * /*engine*/,
                               struct Depsgraph * /*depsgraph*/) {
      npr_cache_populate(vedata, ob);
    };
    DRW_render_object_iter(vedata, engine, depsgraph, npr_render_cache);
    npr_cache_finish(vedata);

    DRW_manager_get()->end_sync();

    /* Also we weed to have a correct FBO bound for #DRW_curves_update */
    // GPU_framebuffer_bind(dfbl->default_fb);
    // DRW_curves_update(); /* TODO(@pragma37): Check this once curves are implemented */

    npr_draw_scene(vedata);

    /* Perform render step between samples to allow
     * flushing of freed GPUBackend resources. */
    GPU_render_step();
    GPU_FINISH_DELIMITER();
  } while (false); /* Sample count goes here */

  const char *viewname = RE_GetActiveRenderView(engine->re);
  write_render_color_output(layer, viewname, dfbl->default_fb, rect);
  write_render_z_output(layer, viewname, dfbl->default_fb, rect, winmat);
}

static void npr_render_update_passes(RenderEngine *engine, Scene *scene, ViewLayer *view_layer)
{
  if (view_layer->passflag & SCE_PASS_COMBINED) {
    RE_engine_register_pass(engine, scene, view_layer, RE_PASSNAME_COMBINED, 4, "RGBA", SOCK_RGBA);
  }
  if (view_layer->passflag & SCE_PASS_Z) {
    RE_engine_register_pass(engine, scene, view_layer, RE_PASSNAME_Z, 1, "Z", SOCK_FLOAT);
  }
}

extern "C" {

static const DrawEngineDataSize npr_data_size = DRW_VIEWPORT_DATA_SIZE(NPR_Data);

DrawEngineType draw_engine_npr = {
    nullptr,
    nullptr,
    N_("NPR"),
    &npr_data_size,
    &npr_engine_init,
    nullptr,
    &npr_instance_free,
    &npr_cache_init,
    &npr_cache_populate,
    &npr_cache_finish,
    &npr_draw_scene,
    &npr_view_update,
    &npr_id_update,
    &npr_render_to_image,
    nullptr,
};

RenderEngineType DRW_engine_viewport_npr_type = {
    nullptr,
    nullptr,
    "BLENDER_NPR",
    N_("NPR"),
    RE_INTERNAL | RE_USE_STEREO_VIEWPORT | RE_USE_GPU_CONTEXT,
    nullptr,
    &DRW_render_to_image,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &npr_render_update_passes,
    &draw_engine_npr,
    {nullptr, nullptr, nullptr},
};
}

/** \} */
