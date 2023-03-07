/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 */

#include "BLI_vector.hh"

#include "eevee_instance.hh"
#include "eevee_subsurface.hh"

#include <iostream>

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Subsurface
 *
 * \{ */

/* TODO(fclem) Only enable this module if there is any SSS object in the scene. */
void SubsurfaceModule::end_sync()
{
  data_.jitter_threshold = inst_.scene->eevee.sss_jitter_threshold;
  if (data_.sample_len != inst_.scene->eevee.sss_samples) {
    /* Convert sample count from old implementation which was using a separable filter. */
    /* TODO(fclem) better remapping. */
    // data_.sample_len = square_f(1 + 2 * inst_.scene->eevee.sss_samples);
    data_.sample_len = 55;
  }

  if (transmittance_tx == nullptr) {
    precompute_transmittance_profile();
  }

  precompute_samples_location();

  data_.push_update();
}

void SubsurfaceModule::precompute_samples_location()
{
  /* Precompute sample position with white albedo. */
  float d = burley_setup(1.0f, 1.0f);

  float rand_u = inst_.sampling.rng_get(SAMPLING_SSS_U);
  float rand_v = inst_.sampling.rng_get(SAMPLING_SSS_V);

  double golden_angle = M_PI * (3.0 - sqrt(5.0));
  for (auto i : IndexRange(data_.sample_len)) {
    float theta = golden_angle * i + M_PI * 2.0f * rand_u;
    /* Scale using rand_v in order to keep first sample always at center. */
    float x = (1.0f + (rand_v / data_.sample_len)) * (i / (float)data_.sample_len);
    float r = burley_sample(d, x);
    data_.samples[i].x = cosf(theta) * r;
    data_.samples[i].y = sinf(theta) * r;
    data_.samples[i].z = 1.0f / burley_pdf(d, r);
  }
}

void SubsurfaceModule::precompute_transmittance_profile()
{
  Vector<float> profile(SSS_TRANSMIT_LUT_SIZE);

  /* Precompute sample position with white albedo. */
  float radius = 1.0f;
  float d = burley_setup(radius, 1.0f);

  /* For each distance d we compute the radiance incoming from an hypothetical parallel plane. */
  for (auto i : IndexRange(SSS_TRANSMIT_LUT_SIZE)) {
    /* Distance from the lit surface plane.
     * Compute to a larger maximum distance to have a smoother falloff for all channels. */
    float lut_radius = SSS_TRANSMIT_LUT_RADIUS * radius;
    float distance = lut_radius * (i + 1e-5f) / profile.size();
    /* Compute radius of the footprint on the hypothetical plane. */
    float r_fp = sqrtf(square_f(lut_radius) - square_f(distance));

    profile[i] = 0.0f;
    float area_accum = 0.0f;
    for (auto j : IndexRange(SSS_TRANSMIT_LUT_STEP_RES)) {
      /* Compute distance to the "shading" point through the medium. */
      float r = (r_fp * (j + 0.5f)) / SSS_TRANSMIT_LUT_STEP_RES;
      float r_prev = (r_fp * (j + 0.0f)) / SSS_TRANSMIT_LUT_STEP_RES;
      float r_next = (r_fp * (j + 1.0f)) / SSS_TRANSMIT_LUT_STEP_RES;
      r = hypotf(r, distance);
      float R = burley_eval(d, r);
      /* Since the profile and configuration are radially symmetrical we
       * can just evaluate it once and weight it accordingly */
      float disk_area = square_f(r_next) - square_f(r_prev);

      profile[i] += R * disk_area;
      area_accum += disk_area;
    }
    /* Normalize over the disk. */
    profile[i] /= area_accum;
  }
  /* Make a smooth gradient from 1 to 0. */
  float range = profile.first() - profile.last();
  float offset = profile.last();
  for (float &value : profile) {
    value = (value - offset) / range;
  }
  profile.first() = 1;
  profile.last() = 0;

  transmittance_tx = GPU_texture_create_1d("SSSTransmittanceProfile",
                                           profile.size(),
                                           1,
                                           GPU_R16F,
                                           GPU_TEXTURE_USAGE_SHADER_READ,
                                           profile.data());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Christensen-Burley SSS model
 *
 * Based on: "Approximate Reflectance Profiles for Efficient Subsurface Scattering"
 * by Per Christensen
 * https://graphics.pixar.com/library/ApproxBSSRDF/approxbssrdfslides.pdf
 * \{ */

float SubsurfaceModule::burley_setup(float radius, float albedo)
{
  float A = albedo;
  /* Diffuse surface transmission, equation (6). */
  float s = 1.9f - A + 3.5f * square_f(A - 0.8f);
  /* Mean free path length adapted to fit ancient Cubic and Gaussian models. */
  float l = 0.25 * M_1_PI * radius;

  return l / s;
}

float SubsurfaceModule::burley_sample(float d, float x_rand)
{
  x_rand *= SSS_BURLEY_TRUNCATE_CDF;

  const float tolerance = 1e-6;
  const int max_iteration_count = 10;
  /* Do initial guess based on manual curve fitting, this allows us to reduce
   * number of iterations to maximum 4 across the [0..1] range. We keep maximum
   * number of iteration higher just to be sure we didn't miss root in some
   * corner case.
   */
  float r;
  if (x_rand <= 0.9) {
    r = exp(x_rand * x_rand * 2.4) - 1.0;
  }
  else {
    /* TODO(sergey): Some nicer curve fit is possible here. */
    r = 15.0;
  }
  /* Solve against scaled radius. */
  for (int i = 0; i < max_iteration_count; i++) {
    float exp_r_3 = exp(-r / 3.0);
    float exp_r = exp_r_3 * exp_r_3 * exp_r_3;
    float f = 1.0 - 0.25 * exp_r - 0.75 * exp_r_3 - x_rand;
    float f_ = 0.25 * exp_r + 0.25 * exp_r_3;

    if (abs(f) < tolerance || f_ == 0.0) {
      break;
    }

    r = r - f / f_;
    if (r < 0.0) {
      r = 0.0;
    }
  }

  return r * d;
}

float SubsurfaceModule::burley_eval(float d, float r)
{
  if (r >= SSS_BURLEY_TRUNCATE * d) {
    return 0.0;
  }
  /* Slide 33. */
  float exp_r_3_d = expf(-r / (3.0f * d));
  float exp_r_d = exp_r_3_d * exp_r_3_d * exp_r_3_d;
  return (exp_r_d + exp_r_3_d) / (8.0f * (float)M_PI * d);
}

float SubsurfaceModule::burley_pdf(float d, float r)
{
  return burley_eval(d, r) / SSS_BURLEY_TRUNCATE_CDF;
}

/** \} */

}  // namespace blender::eevee
