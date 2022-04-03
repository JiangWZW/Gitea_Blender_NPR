/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Intel Corporation */

#pragma once

#ifdef WITH_ONEAPI

#  include "device/kernel.h"
#  include "device/memory.h"
#  include "device/queue.h"
#  include <set>

#  include "device/oneapi/device.h"

CCL_NAMESPACE_BEGIN

class OneapiDevice;
class device_memory;

/* Base class for Oneapi queues. */
class OneapiDeviceQueue : public DeviceQueue {
 public:
  OneapiDeviceQueue(OneapiDevice *device);
  ~OneapiDeviceQueue();

  virtual int num_concurrent_states(const size_t state_size) const override;

  virtual int num_concurrent_busy_states() const override;

  virtual void init_execution() override;

  virtual bool kernel_available(DeviceKernel kernel) const override;

  virtual bool enqueue(DeviceKernel kernel,
                       const int kernel_work_size,
                       DeviceKernelArguments const &args) override;

  virtual bool synchronize() override;

  virtual void zero_to_device(device_memory &mem) override;
  virtual void copy_to_device(device_memory &mem) override;
  virtual void copy_from_device(device_memory &mem) override;

 protected:
  OneapiDevice *oneapi_device;
  oneAPIDLLInterface oneapi_dll;
  KernelContext *kernel_context;
  static std::set<DeviceKernel> SUPPORTED_KERNELS;
  bool with_kernel_statistics;
};

CCL_NAMESPACE_END

#endif /* WITH_ONEAPI */
